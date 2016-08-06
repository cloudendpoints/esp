// Copyright (C) Endpoints Server Proxy Authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//
////////////////////////////////////////////////////////////////////////////////
//
package deploy

import (
	"bytes"
	"crypto/tls"
	"io/ioutil"
	"log"
	"net/http"
	"os/exec"
	"path"
	"strings"
	"text/template"
	"time"
)

const kubectl = "kubectl"
const minikube = "minikube"

type Deployment struct {
	// Backend
	BackendImage   string
	BackendPort    string
	BackendAddress string

	// ESP
	ESPImage      string
	ESPPort       string
	ESPStatusPort string
	ESPAddress    string
	NginxFile     string
	ESPSsl        bool
	SSLKey        string
	SSLCert       string

	// Service config
	ServiceName    string
	ServiceVersion string
	ServiceAPIKey  string
	ServiceFile    string
	ServiceToken   string

	// Kubernetes
	K8sNamespace string
	K8sYamlFile  string
	K8sService   string
	K8sType      string

	// Phase control
	RunDeploy   bool
	RunTest     bool
	RunTearDown bool
}

func (d *Deployment) Basename(file string) string {
	return path.Base(file)
}

func (d *Deployment) ESPUrl() string {
	if d.ESPSsl {
		return "https://" + d.ESPAddress + ":" + d.ESPPort
	} else {
		return "http://" + d.ESPAddress + ":" + d.ESPPort
	}
}

func (d *Deployment) Run(test func(*Deployment)) {
	// configure cluster
	d.Configure()

	// deploy to cluster
	if d.RunDeploy {
		d.Deploy()
	}

	// wait for endpoints to be ready
	d.WaitReady()

	if d.RunTest {
		test(d)
	}

	// tear down the cluster
	if d.RunTearDown {
		d.CollectLogs()
		d.TearDown()
	}
}

// Read and fill out file content
func (d *Deployment) readFile(file string) string {
	t, err := ioutil.ReadFile(file)
	if err != nil {
		panic(err)
	}
	tmpl := template.Must(template.New(file).Parse(string(t)))
	var doc bytes.Buffer
	err = tmpl.Execute(&doc, d)
	if err != nil {
		panic(err)
	}
	return doc.String()
}

// Read file template, fill it, and write it to file
func (d *Deployment) writeTemplate(file string) {
	log.Println("Write config file", file)
	content := d.readFile(file + ".template")
	err := ioutil.WriteFile(file, []byte(content), 0644)
	if err != nil {
		panic(err)
	}
}

// Create configuration files from templates
func (d *Deployment) Configure() {
	// Create YAML config file
	d.writeTemplate(d.K8sYamlFile)

	// Create Nginx config file
	d.writeTemplate(d.NginxFile)

	// Create service config
	d.writeTemplate(d.ServiceFile)
}

// Create a kubernetes cluster and push configuration
func (d *Deployment) Deploy() {
	// Create namespace
	if !CreateNamespace(d.K8sNamespace) {
		panic("Cannot create fresh namespace")
	}

	var err error
	// Push nginx config
	err, _ = Run(kubectl, "create", "configmap", "nginx-config",
		"--namespace", d.K8sNamespace,
		"--from-file", d.NginxFile)
	if err != nil {
		panic("Cannot create configmap")
	}

	// Push service config
	err, _ = Run(kubectl, "create", "configmap", "service-config",
		"--namespace", d.K8sNamespace,
		"--from-file", d.ServiceFile)
	if err != nil {
		panic("Cannot create configmap")
	}

	// Push service account token
	if d.ServiceToken != "" {
		err, _ = Run(kubectl, "create", "secret", "generic", "service-token",
			"--namespace", d.K8sNamespace,
			"--from-file", d.ServiceToken)
		if err != nil {
			panic("Cannot create secret")
		}
	}

	// Push SSL certs
	if d.ESPSsl {
		err, _ = Run(kubectl, "create", "secret", "generic", "nginx-ssl",
			"--from-file", d.SSLKey,
			"--from-file", d.SSLCert,
			"--namespace", d.K8sNamespace)
		if err != nil {
			panic("Cannot create secret")
		}
	}

	// Push YAML
	err, _ = Run(kubectl, "create", "-f", d.K8sYamlFile,
		"--namespace", d.K8sNamespace)
	if err != nil {
		panic("Cannot create k8s")
	}
}

// Wait till all services are ready
func (d *Deployment) WaitReady() {
	// Get IP of the cluster
	var ok bool
	switch d.K8sType {
	case "LoadBalancer":
		ok = Repeat(func() bool {
			err, output := Run(kubectl, "get", "service", d.K8sService,
				"--namespace", d.K8sNamespace, "-o=custom-columns=:.status.loadBalancer.ingress[0].ip")
			if err != nil {
				return false
			} else {
				ip := strings.TrimSpace(output)
				d.ESPAddress = ip
				d.BackendAddress = ip
				return true
			}
		})
	case "NodePort":
		ok = Repeat(func() bool {
			err, output := Run(minikube, "ip")
			if err != nil {
				panic("Missing minikube executable")
			}
			ip := strings.TrimSpace(output)
			d.ESPAddress = ip
			d.BackendAddress = ip

			// Obtain node port translation map
			// Order from YAML file: ESPPort, ESPStatusPort, BackendPort
			err, port0 := Run(kubectl, "get", "service", d.K8sService,
				"--namespace", d.K8sNamespace, "-o=jsonpath={.spec.ports[0].NodePort}")
			if err != nil {
				return false
			}
			d.ESPPort = port0

			err, port1 := Run(kubectl, "get", "service", d.K8sService,
				"--namespace", d.K8sNamespace, "-o=jsonpath={.spec.ports[1].NodePort}")
			if err != nil {
				return false
			}
			d.ESPStatusPort = port1

			err, port2 := Run(kubectl, "get", "service", d.K8sService,
				"--namespace", d.K8sNamespace, "-o=jsonpath={.spec.ports[2].NodePort}")
			if err != nil {
				return false
			}
			d.BackendPort = port2

			return true
		})
	default:
		panic("Unknown kubernetes type")
	}
	if !ok {
		panic("Cannot get IP of the k8s deployment")
	}

	// Check that backends work
	d.WaitESP()
	d.WaitBackend()
}

// Check ESP service
func (d *Deployment) WaitESP() bool {
	return Repeat(func() bool {
		return HTTPGet("http://" + d.ESPAddress + ":" + d.ESPStatusPort + "/endpoints_status")
	})
}

// Check backend service
func (d *Deployment) WaitBackend() bool {
	return Repeat(func() bool {
		return HTTPGet("http://" + d.BackendAddress + ":" + d.BackendPort + "/shelves")
	})
}

// Get logs from all pods and containers
func (d *Deployment) CollectLogs() {
	for _, pod := range d.GetPods() {
		_, _ = Run(kubectl, "logs", pod, "esp", "--namespace", d.K8sNamespace)
		_, _ = Run(kubectl, "logs", pod, "bookstore", "--namespace", d.K8sNamespace)
	}
}

// Get pod names
func (d *Deployment) GetPods() []string {
	_, pods := Run(kubectl, "get", "pods",
		"--namespace", d.K8sNamespace,
		"-o=jsonpath={.items[*].metadata.name}")
	return strings.Fields(pods)
}

// Get container names in a pod
func (d *Deployment) GetContainers(pod string) []string {
	_, names := Run(kubectl, "get", "pods", pod,
		"--namespace", d.K8sNamespace,
		"-o=jsonpath={.spec.containers[*].name}")
	return strings.Fields(names)
}

// Attempt to shut down
func (d *Deployment) TearDown() bool {
	return DeleteNamespace(d.K8sNamespace)
}

// Execute command and return the error code, merged output from stderr and stdout
func Run(name string, args ...string) (error, string) {
	log.Println(">", name, strings.Join(args, " "))
	c := exec.Command(name, args...)
	bytes, err := c.Output()
	if err != nil {
		log.Println(err)
	}
	s := string(bytes)
	log.Println(s)
	return err, s
}

// Repeat until success (function returns true) up to MaxTries
const MaxTries = 10

func Repeat(f func() bool) bool {
	try := 0
	delay := 2 * time.Second
	result := false
	for !result && try < MaxTries {
		if try > 0 {
			log.Println("Waiting for next attempt: ", delay)
			time.Sleep(delay)
			delay = 2 * delay
			log.Println("Repeat attempt #", try+1)
		}
		result = f()
		try = try + 1
	}

	if !result {
		log.Println("Failed all attempts")
	}

	return result
}

func HTTPGet(url string) bool {
	log.Println("HTTP GET", url)
	timeout := time.Duration(5 * time.Second)
	client := &http.Client{
		Timeout: timeout,
		Transport: &http.Transport{
			TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
		},
	}
	resp, err := client.Get(url)
	if err != nil {
		log.Println(err)
		return false
	}
	defer resp.Body.Close()
	body, _ := ioutil.ReadAll(resp.Body)
	log.Println(string(body))
	return true
}

// Check if namespace exists
func ExistsNamespace(namespace string) bool {
	err, _ := Run(kubectl, "get", "namespace", namespace)
	return err == nil
}

// Delete namespace (and wait for completion)
func DeleteNamespace(namespace string) bool {
	Run(kubectl, "delete", "namespace", namespace)
	return Repeat(func() bool {
		return !ExistsNamespace(namespace)
	})
}

// Create a fresh namespace; delete if it already exists
func CreateNamespace(namespace string) bool {
	// Delete namespace if it exists
	if ExistsNamespace(namespace) {
		if !DeleteNamespace(namespace) {
			log.Println("Cannot delete namespace ", namespace)
			return false
		}
	}

	err, _ := Run(kubectl, "create", "namespace", namespace)
	return err == nil
}
