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
////////////////////////////////////////////////////////////////////////////////

package cli

import (
	"deploy"
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	"reflect"
	"strconv"
	"strings"
	"utils"

	"github.com/spf13/cobra"

	api "k8s.io/client-go/1.4/pkg/api/v1"
	extensions "k8s.io/client-go/1.4/pkg/apis/extensions/v1beta1"
	"k8s.io/client-go/1.4/pkg/util/intstr"
)

// Ports are named ports in use by ESP
type Ports struct {
	ssl, http, http2, status int
}

var (
	ports Ports

	image string

	sslKey   string
	sslCerts string

	config deploy.Service
	svc    *api.Service
)

// Default number of replicas for ESP service
const defaultReplicas = 1

var deployCmd = &cobra.Command{
	Use:   "deploy [kubernetes service] [optional name for ESP service]",
	Short: "Deploy ESP for a service as a sidecar or a separate service",
	Run: func(cmd *cobra.Command, args []string) {
		// Must take one parameter
		if len(args) == 0 {
			fmt.Println("Please specify kubernetes service name and optionally ESP service name")
			os.Exit(-1)
		}

		// First argument is the service name
		var err error
		name := args[0]
		svc, err = clientset.Core().Services(namespace).Get(name)
		if err != nil {
			fmt.Println("Cannot find kubernetes service", err)
			os.Exit(-1)
		}

		err = setServiceConfig()
		if err != nil {
			fmt.Println(err)
			os.Exit(-1)
		}

		// Second argument is the name for the ESP service
		if len(args) > 1 {
			app := args[1]
			port, err := validateBackendPort(false)
			if err != nil {
				fmt.Println(err)
				os.Exit(-2)
			}
			backend := svc.Name + ":" + strconv.Itoa(port)
			err = CreateService(app, ports, api.ServiceTypeNodePort)
			if err != nil {
				fmt.Println(err)
				os.Exit(-2)
			}
			err = CreateDeployment(app, ports, backend)
			if err != nil {
				fmt.Println(err)
				os.Exit(-2)
			}
		} else {
			port, err := validateBackendPort(true)
			if err != nil {
				fmt.Println(err)
				os.Exit(-2)
			}
			backend := "localhost:" + strconv.Itoa(port)
			err = InjectService(ports)
			if err != nil {
				fmt.Println(err)
				os.Exit(-2)
			}
			err = InjectDeployment(ports, backend)
			if err != nil {
				fmt.Println(err)
				os.Exit(-2)
			}
		}
	},
}

func init() {
	RootCmd.AddCommand(deployCmd)
	deployCmd.PersistentFlags().StringVar(&image,
		"image",
		"b.gcr.io/endpoints/endpoints-runtime:0.3.6",
		"URL to ESP docker image")

	deployCmd.PersistentFlags().StringVarP(&config.Name,
		"service", "s", "", "API service name, empty to use the service label")
	deployCmd.PersistentFlags().StringVarP(&config.Version,
		"version", "v", "", "API service config version, empty to use the latest")
	deployCmd.PersistentFlags().StringVarP(&config.CredentialsFile,
		"creds", "k", "", "Service account key JSON file")

	deployCmd.PersistentFlags().IntVarP(&ports.status,
		"status", "N", 8090, "Status port for ESP")
	deployCmd.PersistentFlags().IntVarP(&ports.http,
		"http", "p", 8080, "HTTP/1.x port to use for ESP")
	deployCmd.PersistentFlags().IntVarP(&ports.http2,
		"http2", "P", 0, "HTTP/2 port to use for ESP")
	deployCmd.PersistentFlags().IntVarP(&ports.ssl,
		"ssl", "S", 443, "HTTPS port to use for ESP")

	deployCmd.PersistentFlags().StringVar(&sslKey,
		"sslKey", utils.SSLKeyFile(), "SSL key file")
	deployCmd.PersistentFlags().StringVar(&sslCerts,
		"sslCerts", utils.SSLCertFile(), "SSL certs file")
}

func setServiceConfig() (err error) {
	if config.Name == "" {
		name, ok := svc.Labels[ESPConfig]
		if !ok {
			return errors.New("Missing service name")
		}
		config.Name = name
	}

	// Fetch latest service config version
	if config.Version == "" {
		config.Version, err = config.GetVersion()
		if err != nil {
			return
		}
	}

	return nil
}

func validateBackendPort(target bool) (int, error) {
	svcPorts := filterServicePorts(svc.Spec.Ports)
	if len(svcPorts) != 1 {
		return 0, errors.New("Service does not expose a single port")
	}
	svcPort := svcPorts[0]

	var port int
	if target {
		if svcPort.TargetPort.Type != intstr.Int {
			return 0, errors.New("Service target port should not be a named port")
		}

		// Check for collisions on the service pods with the port
		port = int(svcPort.TargetPort.IntVal)
		if port == ports.http || port == ports.http2 ||
			port == ports.ssl || port == ports.status {
			return 0, errors.New("Service target port should be distinct from ESP ports: " +
				strconv.Itoa(port))
		}
	} else {
		port = int(svcPort.Port)
	}
	return port, nil
}

func InjectService(ports Ports) error {
	// update target port for service
	svcPorts := filterServicePorts(svc.Spec.Ports)

	// add a port name if missing (required by kubernetes)
	if svcPorts[0].Name == "" {
		svcPorts[0].Name = "backend"
	}

	svc.Spec.Ports = append(CreateServicePorts(ports), svcPorts[0])
	_, err := clientset.Core().Services(namespace).Update(svc)
	if err != nil {
		return err
	}
	svc.Labels[ESPManagedService] = svc.Name

	fmt.Println("Updated service", svc.Name)
	return nil
}

func InjectDeployment(ports Ports, backend string) error {
	// find deployment by service name
	dpl, err := clientset.Extensions().Deployments(namespace).Get(svc.Name)
	if err != nil {
		return err
	}

	// create an ESP container
	esp, volumes, err := MakeContainer(ports, backend)
	if err != nil {
		return err
	}

	// update deployment
	// TODO garbage collect old secrets
	template := &dpl.Spec.Template
	template.Spec.Containers = append(
		filterContainers(template.Spec.Containers), *esp)
	template.Spec.Volumes = append(
		filterVolumes(template.Spec.Volumes), volumes...)
	template.Labels[ESPManagedService] = svc.Name

	// commit updates
	_, err = clientset.Extensions().Deployments(namespace).Update(dpl)
	if err != nil {
		return err
	}
	fmt.Println("Updated deployment", dpl.Name)
	return nil
}

// MakeContainer from ESP image
func MakeContainer(ports Ports, backend string) (*api.Container, []api.Volume, error) {
	// Create config volumes and push data
	var volumeMounts = make([]api.VolumeMount, 0)
	var volumes = make([]api.Volume, 0)
	var args = []string{
		"-s", config.Name,
		"-v", config.Version,
		"-a", backend,
	}
	var published = make([]api.ContainerPort, 0)

	if ports.http > 0 {
		args = append(args, "-p", strconv.Itoa(ports.http))
		published = append(published, api.ContainerPort{ContainerPort: int32(ports.http)})
	}

	if ports.http2 > 0 {
		args = append(args, "-P", strconv.Itoa(ports.http2))
		published = append(published, api.ContainerPort{ContainerPort: int32(ports.http2)})
	}

	if ports.ssl > 0 {
		args = append(args, "-S", strconv.Itoa(ports.ssl))
		published = append(published, api.ContainerPort{ContainerPort: int32(ports.ssl)})

		name, err := AddSecret(endpoints+"-ssl-", map[string]string{
			"nginx.key": sslKey,
			"nginx.crt": sslCerts,
		})

		if err != nil {
			return nil, nil, err
		}

		volumeMounts = append(volumeMounts,
			api.VolumeMount{Name: name, MountPath: "/etc/nginx/ssl", ReadOnly: true})
		volumes = append(volumes, makeSecretVolume(name))
	}

	if ports.status > 0 {
		args = append(args, "-N", strconv.Itoa(ports.status))
		published = append(published, api.ContainerPort{ContainerPort: int32(ports.status)})
	}

	if config.CredentialsFile != "" {
		args = append(args, "-k", "/etc/nginx/creds/secret.json")

		name, err := AddSecret(endpoints+"-creds-", map[string]string{
			"secret.json": config.CredentialsFile,
		})

		if err != nil {
			return nil, nil, err
		}

		volumeMounts = append(volumeMounts,
			api.VolumeMount{Name: name, MountPath: "/etc/nginx/creds", ReadOnly: true})
		volumes = append(volumes, makeSecretVolume(name))
	}

	esp := api.Container{
		Name:         endpoints,
		Image:        image,
		Ports:        published,
		VolumeMounts: volumeMounts,
		Args:         args,
	}

	return &esp, volumes, nil
}

// CreateDeployment for ESP pods
func CreateDeployment(app string, ports Ports, backend string) error {
	esp, volumes, err := MakeContainer(ports, backend)
	if err != nil {
		return err
	}

	replicas := int32(defaultReplicas)

	// Create replication controller
	deployment := &extensions.Deployment{
		ObjectMeta: api.ObjectMeta{
			Name: app,
		},
		Spec: extensions.DeploymentSpec{
			Replicas: &replicas,
			Template: api.PodTemplateSpec{
				ObjectMeta: api.ObjectMeta{
					Labels: map[string]string{
						ESPManagedService: svc.Name,
						ESPEndpoints:      app,
					},
				},
				Spec: api.PodSpec{
					Containers: []api.Container{*esp},
					Volumes:    volumes,
				},
			},
		},
	}

	deployment, err = clientset.Extensions().Deployments(namespace).Create(deployment)
	if err != nil {
		return err
	}
	fmt.Println("Created deployment", deployment.Name)
	return nil
}

func CreateServicePorts(ports Ports) []api.ServicePort {
	var published = make([]api.ServicePort, 0)
	v := reflect.ValueOf(ports)
	for i := 0; i < v.NumField(); i++ {
		port := int(v.Field(i).Int())
		if port > 0 {
			published = append(published, api.ServicePort{
				Name:     endpoints + "-" + v.Type().Field(i).Name,
				Protocol: api.ProtocolTCP,
				Port:     int32(port),
			})
		}
	}
	return published
}

// CreateService for ESP pods
func CreateService(app string, ports Ports, serviceType api.ServiceType) error {
	// publish all ports
	svc := &api.Service{
		ObjectMeta: api.ObjectMeta{
			Name: app,
			Labels: map[string]string{
				ESPManagedService: svc.Name,
			},
		},
		Spec: api.ServiceSpec{
			Ports: CreateServicePorts(ports),
			Selector: map[string]string{
				ESPManagedService: svc.Name,
				ESPEndpoints:      app,
			},
			Type: serviceType,
		},
	}

	svc, err := clientset.Core().Services(namespace).Create(svc)
	if err != nil {
		return err
	}
	fmt.Println("Created service", svc.Name)
	return nil
}

// AddConfig from a map of files
func AddConfig(key string, files map[string]string) (string, error) {
	m := map[string]string{}
	for k, fileName := range files {
		data, err := ioutil.ReadFile(fileName)
		if err != nil {
			fmt.Println("Cannot find config file", fileName)
			return "", err
		}
		m[k] = string(data)
	}

	config := &api.ConfigMap{
		ObjectMeta: api.ObjectMeta{
			GenerateName: key,
		},
		Data: m,
	}

	configMaps := clientset.Core().ConfigMaps(namespace)
	configMap, err := configMaps.Create(config)
	if err != nil {
		return "", err
	}
	fmt.Printf("Created config map %s from %v\n", configMap.Name, files)
	return configMap.Name, nil
}

// AddSecret using a given key
func AddSecret(key string, files map[string]string) (string, error) {
	m := map[string][]byte{}
	for k, fileName := range files {
		data, err := ioutil.ReadFile(fileName)
		if err != nil {
			fmt.Println("Cannot find secret file", fileName)
			return "", err
		}

		m[k] = data

	}

	config := &api.Secret{
		ObjectMeta: api.ObjectMeta{
			GenerateName: key,
		},
		Data: m,
	}

	secrets := clientset.Core().Secrets(namespace)
	secret, err := secrets.Create(config)
	if err != nil {
		return "", err
	}
	fmt.Printf("Created secret %s from %v\n", secret.Name, files)
	return secret.Name, nil
}

// Create a secret volume
func makeSecretVolume(name string) api.Volume {
	return api.Volume{
		Name: name,
		VolumeSource: api.VolumeSource{
			Secret: &api.SecretVolumeSource{
				SecretName: name,
			},
		},
	}
}

// Create a volume
func makeVolume(name string) api.Volume {
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

func filterVolumes(volumes []api.Volume) []api.Volume {
	var out = make([]api.Volume, 0)
	for _, v := range volumes {
		if !strings.HasPrefix(v.Name, endpoints) {
			out = append(out, v)
		}
	}
	return out
}

func filterContainers(containers []api.Container) []api.Container {
	var out = make([]api.Container, 0)
	for _, v := range containers {
		if !strings.HasPrefix(v.Name, endpoints) {
			out = append(out, v)
		}
	}
	return out
}

func filterServicePorts(ports []api.ServicePort) []api.ServicePort {
	var out = make([]api.ServicePort, 0)
	for _, v := range ports {
		if !strings.HasPrefix(v.Name, endpoints) {
			out = append(out, v)
		}
	}
	return out
}
