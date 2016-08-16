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
	"crypto/tls"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"path"
	"strconv"
	"time"

	"k8s.io/kubernetes/pkg/api"
	"k8s.io/kubernetes/pkg/client/restclient"
	client "k8s.io/kubernetes/pkg/client/unversioned"
)

const app = "esp-backend"

type KubernetesService struct {
	Name       string
	Image      string
	Port       int
	SSLPort    int
	StatusPort int
	// URL of the health status location (e.g. /endpoints_status)
	Status string
}

type Deployment struct {
	Backend KubernetesService
	ESP     KubernetesService

	// ESP
	NginxFile string
	SSLKey    string
	SSLCert   string

	// Service config
	ServiceName    string
	ServiceVersion string
	ServiceAPIKey  string
	ServiceToken   string

	// Kubernetes
	K8sNamespace string
	K8sPort      int
	Minikube     string
	c            *client.Client

	// Phase control
	RunDeploy   bool
	RunTest     bool
	RunTearDown bool
}

// Extract the base name of the file
func (d *Deployment) Basename(file string) string {
	return path.Base(file)
}

// Run test given the URL of ESP endpoint
func (d *Deployment) Run(test func(string)) {
	// connect to k8s API
	log.Println("Connecting to kubectl proxy")
	config := restclient.Config{
		Host: fmt.Sprintf("localhost:%d", d.K8sPort),
	}
	var err error
	d.c, err = client.New(&config)
	if err != nil {
		log.Fatalln("Can't connect to Kubernetes API: ", err)
	}

	// deploy to cluster
	if d.RunDeploy {
		d.Deploy()
	}

	// run test
	if d.RunTest {
		addr := d.WaitReady()
		test(addr)
	}

	// tear down the cluster
	if d.RunTearDown {
		d.CollectLogs()
		d.TearDown()
	}
}

func (d *Deployment) addConfig(key string, fileNames ...string) bool {
	log.Printf("Add config %s from %v\n", key, fileNames)

	m := map[string]string{}
	for _, fileName := range fileNames {
		data, err := ioutil.ReadFile(fileName)
		if err != nil {
			log.Println("Cannot find config file", fileName)
			return false
		}
		m[d.Basename(fileName)] = string(data)
	}

	config := &api.ConfigMap{
		ObjectMeta: api.ObjectMeta{
			Name: key,
		},
		Data: m,
	}

	configMaps := d.c.ConfigMaps(d.K8sNamespace)
	_, err := configMaps.Create(config)
	if err != nil {
		log.Println("Cannot create configmap", err)
		return false
	}

	return true
}

func (d *Deployment) addSecret(key string, files map[string]string) bool {
	log.Printf("Add secret %s from %v\n", key, files)

	m := map[string][]byte{}
	for k, fileName := range files {
		data, err := ioutil.ReadFile(fileName)
		if err != nil {
			log.Println("Cannot find secret file", fileName)
			return false
		}
		m[k] = data

	}

	config := &api.Secret{
		ObjectMeta: api.ObjectMeta{
			Name: key,
		},
		Data: m,
	}

	secrets := d.c.Secrets(d.K8sNamespace)
	_, err := secrets.Create(config)
	if err != nil {
		log.Println("Cannot create secret", err)
		return false
	}

	return true
}

func makeVolume(name string) api.Volume {
	log.Println("Make volume", name)
	return api.Volume{
		Name: name,
		VolumeSource: api.VolumeSource{
			ConfigMap: &api.ConfigMapVolumeSource{
				LocalObjectReference: api.LocalObjectReference{
					Name: name,
				},
			},
		},
	}
}

func makeSecretVolume(name string) api.Volume {
	log.Println("Make volume", name)
	return api.Volume{
		Name: name,
		VolumeSource: api.VolumeSource{
			Secret: &api.SecretVolumeSource{
				SecretName: name,
			},
		},
	}
}

func (d *Deployment) port() int {
	if d.ESP.SSLPort > 0 {
		return d.ESP.SSLPort
	} else {
		return d.ESP.Port
	}
}

func (d *Deployment) DeployPods() bool {
	// Create config volumes and push data
	volumeMounts := make([]api.VolumeMount, 0)
	volumes := make([]api.Volume, 0)
	commands := []string{
		"/usr/sbin/start_esp.py",
		"-s", d.ServiceName,
		"-v", d.ServiceVersion,
		"-p", strconv.Itoa(d.ESP.Port),
		"-N", strconv.Itoa(d.ESP.StatusPort),
		"-a", "localhost:" + strconv.Itoa(d.Backend.Port),
	}
	var failed bool

	if d.ServiceToken != "" {
		token := "service-token"
		volumeMounts = append(volumeMounts,
			api.VolumeMount{Name: token, MountPath: "/etc/nginx/creds", ReadOnly: true})
		volumes = append(volumes, makeSecretVolume(token))
		commands = append(commands, "-k", "/etc/nginx/creds/secret.json")
		failed = failed || !d.addSecret(token, map[string]string{
			"secret.json": d.ServiceToken,
		})
	}

	if d.ESP.SSLPort > 0 {
		ssl := "nginx-ssl"
		volumeMounts = append(volumeMounts,
			api.VolumeMount{Name: ssl, MountPath: "/etc/nginx/ssl", ReadOnly: true})
		volumes = append(volumes, makeSecretVolume(ssl))
		commands = append(commands, "-S", strconv.Itoa(d.ESP.SSLPort))
		failed = failed || !d.addSecret(ssl, map[string]string{
			"nginx.key": d.SSLKey,
			"nginx.crt": d.SSLCert,
		})
	}

	if failed {
		log.Println("Cannot push configuration")
		return false
	}

	esp := api.Container{
		Name:  d.ESP.Name,
		Image: d.ESP.Image,
		Ports: []api.ContainerPort{
			api.ContainerPort{ContainerPort: int32(d.port())},
			api.ContainerPort{ContainerPort: int32(d.ESP.StatusPort)},
		},
		VolumeMounts: volumeMounts,
		Command:      commands,
	}

	backend := api.Container{
		Name:  d.Backend.Name,
		Image: d.Backend.Image,
		Ports: []api.ContainerPort{
			api.ContainerPort{ContainerPort: int32(d.Backend.Port)},
		},
	}

	// Create replication controller
	rc := &api.ReplicationController{
		ObjectMeta: api.ObjectMeta{
			Name: app,
		},
		Spec: api.ReplicationControllerSpec{
			Replicas: 1,
			Selector: map[string]string{
				"app": app,
			},
			Template: &api.PodTemplateSpec{
				ObjectMeta: api.ObjectMeta{
					Labels: map[string]string{
						"app": app,
					},
				},
				Spec: api.PodSpec{
					Containers: []api.Container{esp, backend},
					Volumes:    volumes,
				},
			},
		},
	}

	log.Println("Create replication controller")
	_, err := d.c.ReplicationControllers(d.K8sNamespace).Create(rc)
	if err != nil {
		log.Println("Cannot create replication controller", err)
		return false
	}

	return true
}

func (d *Deployment) DeployService() bool {
	st := api.ServiceTypeLoadBalancer
	if d.Minikube != "" {
		// Change to node port for local development
		st = api.ServiceTypeNodePort
	}
	svc := &api.Service{
		ObjectMeta: api.ObjectMeta{
			Name: app,
		},
		Spec: api.ServiceSpec{
			Ports: []api.ServicePort{
				api.ServicePort{Name: "esp", Protocol: api.ProtocolTCP, Port: int32(d.port())},
				api.ServicePort{Name: "status", Protocol: api.ProtocolTCP, Port: int32(d.ESP.StatusPort)},
				api.ServicePort{Name: "backend", Protocol: api.ProtocolTCP, Port: int32(d.Backend.Port)},
			},
			Selector: map[string]string{
				"app": app,
			},
			Type: st,
		},
	}

	log.Println("Create service")
	_, err := d.c.Services(d.K8sNamespace).Create(svc)
	if err != nil {
		log.Println("Cannot create service", err)
		return false
	}

	return true
}

// Create a kubernetes cluster and push configuration
func (d *Deployment) Deploy() bool {
	// Delete namespace if it exists
	if d.ExistsNamespace() {
		if !d.TearDown() {
			log.Println("Cannot delete namespace")
			return false
		}
	}

	log.Println("Create namespace")
	namespace := &api.Namespace{
		ObjectMeta: api.ObjectMeta{
			Name: d.K8sNamespace,
		},
	}
	d.c.Namespaces().Create(namespace)

	// Poll to make sure namespace exists
	if !Repeat(func() bool {
		return d.ExistsNamespace()
	}) {
		log.Println("Cannot create namespace")
		return false
	}

	if !d.DeployPods() {
		return false
	}

	if !d.DeployService() {
		return false
	}

	return true
}

// Wait till all services are ready
func (d *Deployment) WaitReady() string {
	// Get external address
	var ip string
	var port int
	var statusPort int
	var backendPort int

	if d.Minikube != "" {
		ip = d.Minikube

		ok := Repeat(func() bool {
			log.Println("Retrieving ports of the service")
			svc, err := d.c.Services(d.K8sNamespace).Get(app)
			if err != nil {
				return false
			}

			ports := svc.Spec.Ports
			if len(ports) < 3 {
				return false
			}

			port = int(ports[0].NodePort)
			statusPort = int(ports[1].NodePort)
			backendPort = int(ports[2].NodePort)

			return true
		})

		if !ok {
			log.Fatalln("Failed to retrieve ports of the service")
		}

	} else {
		ok := Repeat(func() bool {
			log.Println("Retrieving address of the service")
			svc, err := d.c.Services(d.K8sNamespace).Get(app)
			if err != nil {
				return false
			}

			ingress := svc.Status.LoadBalancer.Ingress
			if len(ingress) == 0 {
				return false
			}

			address := ingress[0]

			if address.IP != "" {
				ip = address.IP
				return true
			} else if address.Hostname != "" {
				ip = address.Hostname
				return true
			}

			return false
		})

		if !ok {
			log.Fatalln("Failed to retrieve IP of the service")
		}

		port = d.ESP.Port
		statusPort = d.ESP.StatusPort
		backendPort = d.Backend.Port

		if d.ESP.SSLPort > 0 {
			port = d.ESP.SSLPort
		}
	}

	log.Println("Address is", ip)

	// Check that backends work
	d.WaitService(ip, statusPort, d.ESP.Status)
	d.WaitService(ip, backendPort, d.Backend.Status)

	if d.ESP.SSLPort > 0 {
		return fmt.Sprintf("https://%s:%d", ip, port)
	} else {
		return fmt.Sprintf("http://%s:%d", ip, port)
	}
}

// Check service
func (d *Deployment) WaitService(ip string, port int, status string) bool {
	return Repeat(func() bool {
		return HTTPGet(fmt.Sprintf("http://%s:%d%s", ip, port, status))
	})
}

// Get logs from all pods and containers
func (d *Deployment) CollectLogs() {
	log.Println("Collect logs")
	var containers = []string{d.ESP.Name, d.Backend.Name}
	for _, pod := range d.GetPods() {
		for _, container := range containers {
			log.Printf("Logs for %s in %s\n", container, pod)
			logOptions := &api.PodLogOptions{
				Container:  container,
				Timestamps: true,
			}
			raw, err := d.c.Pods(d.K8sNamespace).GetLogs(pod, logOptions).Do().Raw()
			if err != nil {
				log.Println("Request error", err)
			} else {
				log.Println("\n" + string(raw))
			}
		}
	}
}

// Get pod names
func (d *Deployment) GetPods() []string {
	list, err := d.c.Pods(d.K8sNamespace).List(api.ListOptions{})
	if err != nil {
		return nil
	}
	var out = make([]string, len(list.Items))
	for i := 0; i < len(list.Items); i++ {
		out[i] = list.Items[i].Name
	}
	return out
}

// Attempt to shut down
func (d *Deployment) TearDown() bool {
	log.Println("Delete namespace", d.K8sNamespace)
	d.c.Namespaces().Delete(d.K8sNamespace)

	// Poll to make sure namespace is deleted
	return Repeat(func() bool {
		return !d.ExistsNamespace()
	})
}

// Check if namespace exists
func (d *Deployment) ExistsNamespace() bool {
	list, err := d.c.Namespaces().List(api.ListOptions{})
	if err != nil {
		return false
	}

	for _, item := range list.Items {
		if item.Name == d.K8sNamespace {
			log.Println("Namespace exists:", d.K8sNamespace)
			return true
		}
	}

	log.Println("Namespace does not exist:", d.K8sNamespace)
	return false
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
