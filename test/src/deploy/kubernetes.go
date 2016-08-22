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
	"fmt"
	"io/ioutil"
	"log"
	"strconv"
	"utils"

	"k8s.io/kubernetes/pkg/api"
	"k8s.io/kubernetes/pkg/client/restclient"
	client "k8s.io/kubernetes/pkg/client/unversioned"
)

const app = "esp-backend"

type Application struct {
	Name string
	// Docker image URL
	Image string
	Port  int
	// Optional SSL port
	SSLPort int
	// Status port for querying Status
	StatusPort int
	// URL of the health status location (e.g. /endpoints_status)
	Status string
}

type Deployment struct {
	Backend *Application
	ESP     *Application
	Config  *Service

	GRPC      bool
	SSLKey    string
	SSLCert   string
	IP        string
	c         *client.Client
	namespace string
}

func (d *Deployment) Init(port int, namespace string) {
	// connect to k8s API
	log.Println("Connecting to kubectl proxy at port:", port)
	config := restclient.Config{
		Host: fmt.Sprintf("localhost:%d", port),
	}
	var err error

	d.c, err = client.New(&config)
	if err != nil {
		log.Fatalln("Can't connect to Kubernetes API: ", err)
	}

	d.namespace = namespace
}

func (d *Deployment) addConfig(key string, files map[string]string) bool {
	log.Printf("Add config %s from %v\n", key, files)

	m := map[string]string{}
	for k, fileName := range files {
		data, err := ioutil.ReadFile(fileName)
		if err != nil {
			log.Println("Cannot find config file", fileName)
			return false
		}
		m[k] = string(data)
	}

	config := &api.ConfigMap{
		ObjectMeta: api.ObjectMeta{
			Name: key,
		},
		Data: m,
	}

	configMaps := d.c.ConfigMaps(d.namespace)
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

	secrets := d.c.Secrets(d.namespace)
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
		"-s", d.Config.Name,
		"-v", d.Config.Version,
		"-N", strconv.Itoa(d.ESP.StatusPort),
		"-a", "localhost:" + strconv.Itoa(d.Backend.Port),
	}
	var failed bool

	if d.GRPC {
		commands = append(commands, "-p", "0",
			"-g", "-P", strconv.Itoa(d.ESP.Port))
	} else {
		commands = append(commands, "-p", strconv.Itoa(d.ESP.Port))
	}

	if d.Config.Token != "" {
		token := "service-token"
		volumeMounts = append(volumeMounts,
			api.VolumeMount{Name: token, MountPath: "/etc/nginx/creds", ReadOnly: true})
		volumes = append(volumes, makeSecretVolume(token))
		commands = append(commands, "-k", "/etc/nginx/creds/secret.json")
		failed = failed || !d.addSecret(token, map[string]string{
			"secret.json": d.Config.Token,
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
	_, err := d.c.ReplicationControllers(d.namespace).Create(rc)
	if err != nil {
		log.Println("Cannot create replication controller", err)
		return false
	}

	return true
}

func (d *Deployment) DeployService() bool {
	st := api.ServiceTypeLoadBalancer
	if d.IP != "" {
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
	_, err := d.c.Services(d.namespace).Create(svc)
	if err != nil {
		log.Println("Cannot create service", err)
		return false
	}

	return true
}

// Create a kubernetes cluster and push configuration
func (d *Deployment) Deploy() bool {
	// Delete namespace if it exists
	if d.namespace != "" && d.ExistsNamespace() {
		if !d.TearDown() {
			log.Println("Cannot delete namespace")
			return false
		}

		// Poll to make sure namespace is deleted
		if !utils.Repeat(func() bool {
			return !d.ExistsNamespace()
		}) {
			log.Println("Cannot delete namespace")
			return false
		}
	}

	log.Println("Create namespace")

	namespace := &api.Namespace{}

	// Request a unique name
	if d.namespace == "" {
		namespace.ObjectMeta.GenerateName = "test-"
	} else {
		namespace.ObjectMeta.Name = d.namespace
	}

	ns, err := d.c.Namespaces().Create(namespace)
	if err != nil {
		log.Println("Cannot create namespace")
		return false
	}

	// Save namespace name for next phases
	if d.namespace == "" {
		d.namespace = ns.ObjectMeta.Name
	}

	// Poll to make sure namespace exists
	if !utils.Repeat(func() bool {
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

// Wait till all services are ready and return ESP URL
func (d *Deployment) WaitReady() string {
	// Get external address
	var ip string
	var port int
	var statusPort int
	var backendPort int

	if d.IP != "" {
		ip = d.IP

		ok := utils.Repeat(func() bool {
			log.Println("Retrieving ports of the service")
			svc, err := d.c.Services(d.namespace).Get(app)
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
		ok := utils.Repeat(func() bool {
			log.Println("Retrieving address of the service")
			svc, err := d.c.Services(d.namespace).Get(app)
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

	if d.GRPC {
		// TODO: create a status port for GRPC backend
	} else {
		d.WaitService(ip, backendPort, d.Backend.Status)
	}

	if d.ESP.SSLPort > 0 {
		return fmt.Sprintf("https://%s:%d", ip, port)
	} else {
		return fmt.Sprintf("http://%s:%d", ip, port)
	}
}

// Check service
func (d *Deployment) WaitService(ip string, port int, status string) bool {
	return utils.Repeat(func() bool {
		return utils.HTTPGet(fmt.Sprintf("http://%s:%d%s", ip, port, status))
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
			raw, err := d.c.Pods(d.namespace).GetLogs(pod, logOptions).Do().Raw()
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
	list, err := d.c.Pods(d.namespace).List(api.ListOptions{})
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
	log.Println("Delete namespace", d.namespace)
	err := d.c.Namespaces().Delete(d.namespace)
	if err != nil {
		log.Println("Cannot delete namespace")
		return false
	}

	return true
}

// Check if namespace exists
func (d *Deployment) ExistsNamespace() bool {
	list, err := d.c.Namespaces().List(api.ListOptions{})
	if err != nil {
		return false
	}

	for _, item := range list.Items {
		if item.Name == d.namespace {
			log.Println("Namespace exists:", d.namespace)
			return true
		}
	}

	log.Println("Namespace does not exist:", d.namespace)
	return false
}
