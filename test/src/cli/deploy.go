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
	"io/ioutil"
	"log"
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

	image       string
	serviceType string

	sslKey          string
	sslCert         string
	customNginxConf string
	customAccessLog string

	cfg deploy.Service
	svc *api.Service

	grpc  bool
	tight bool
)

// Default number of replicas for ESP service
const defaultReplicas = 1

var deployCmd = &cobra.Command{
	Use:   "deploy [source kubernetes service] [target ESP service]",
	Short: "Deploy ESP for a service as a sidecar or a separate service",
	Run: func(cmd *cobra.Command, args []string) {
		// Must take one parameter
		if len(args) != 2 {
			log.Println("Please specify kubernetes service name and ESP service name")
			os.Exit(-1)
		}

		name := args[0]
		app := args[1]

		// First argument is the service name
		var err error
		svc, err = clientset.Core().Services(namespace).Get(name)
		if err != nil {
			log.Println("Cannot find kubernetes service:", err)
			os.Exit(-1)
		}

		err = setServiceConfig(&cfg)
		if err != nil {
			log.Println(err)
			os.Exit(-1)
		}

		backend, err := GetBackend()
		if err != nil {
			log.Println(err)
			os.Exit(-2)
		}

		// selector for ESP pods
		selector, err := CreateDeployment(app, ports, backend)
		if err != nil {
			log.Println(err)
			os.Exit(-2)
		}

		err = CreateService(app, selector, ports, api.ServiceType(serviceType))
		if err != nil {
			log.Println(err)
			os.Exit(-2)
		}
	},
}

func init() {
	RootCmd.AddCommand(deployCmd)
	deployCmd.PersistentFlags().StringVar(&image,
		"image",
		"b.gcr.io/endpoints/endpoints-runtime:0.3.7",
		"URL to ESP docker image")

	deployCmd.PersistentFlags().StringVarP(&cfg.Name,
		"service", "s", "",
		"API service name, empty to use the service label")
	deployCmd.PersistentFlags().StringVarP(&cfg.Version,
		"version", "v", "",
		"API service config version, empty to use the latest")
	deployCmd.PersistentFlags().StringVarP(&cfg.CredentialsFile,
		"creds", "k", "",
		"Service account private key JSON file")

	deployCmd.PersistentFlags().IntVarP(&ports.status,
		"status", "N", 8090, "Status port for ESP (always enabled)")
	deployCmd.PersistentFlags().IntVarP(&ports.http,
		"http", "p", 8080, "HTTP/1.x port to use for ESP")
	deployCmd.PersistentFlags().IntVarP(&ports.http2,
		"http2", "P", 0, "HTTP/2 port to use for ESP")
	deployCmd.PersistentFlags().IntVarP(&ports.ssl,
		"ssl", "S", 0, "HTTPS port to use for ESP")
	deployCmd.PersistentFlags().StringVarP(&customNginxConf,
		"nginx_config", "n", "", "Use a custom nginx config file")
	deployCmd.PersistentFlags().StringVar(&customAccessLog,
		"access_log", "", "Use a custom nginx access log file (or 'off' to disable)")

	deployCmd.PersistentFlags().StringVar(&sslKey,
		"sslKey", utils.SSLKeyFile(), "SSL key file")
	deployCmd.PersistentFlags().StringVar(&sslCert,
		"sslCert", utils.SSLCertFile(), "SSL certificate file")

	deployCmd.PersistentFlags().BoolVarP(&grpc,
		"grpc", "g", false,
		"Use GRPC to communicate with the backend")
	deployCmd.PersistentFlags().BoolVarP(&tight,
		"tight", "t", false,
		"Use a sidecar deployment instead of a separate service")
	deployCmd.PersistentFlags().StringVarP(&serviceType,
		"serviceType", "e", "NodePort",
		"Expose ESP service as the provided service type")
}

func setServiceConfig(config *deploy.Service) (err error) {
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

// GetBackend returns the backend address for ESP service
func GetBackend() (string, error) {
	svcPorts := svc.Spec.Ports
	if len(svcPorts) != 1 {
		return "", errors.New("Service does not expose a single port")
	}
	svcPort := svcPorts[0]

	var backend string
	if tight {
		if svcPort.TargetPort.Type != intstr.Int {
			return "", errors.New("Service target port should not be a named port")
		}

		// Check for collisions on the service pods with the port
		port := int(svcPort.TargetPort.IntVal)
		if port == ports.http || port == ports.http2 ||
			port == ports.ssl || port == ports.status {
			return "", errors.New("Service target port should be distinct from ESP ports: " +
				strconv.Itoa(port))
		}

		backend = "127.0.0.1:" + strconv.Itoa(port)
	} else {
		backend = svc.Name + ":" + strconv.Itoa(int(svcPort.Port))
	}

	if grpc {
		return "grpc://" + backend, nil
	}

	return backend, nil
}

// MakeName defines the name for the ESP container for a kubernetes service
func MakeName(name string) string {
	return endpointsPrefix + name
}

// MakeContainer from ESP image
func MakeContainer(serviceName string, ports Ports, backend string) (*api.Container, []api.Volume, error) {
	// Create config volumes and push data
	var volumeMounts = make([]api.VolumeMount, 0)
	var volumes = make([]api.Volume, 0)
	var published = make([]api.ContainerPort, 0)

	var args = []string{
		"-s", cfg.Name,
		"-v", cfg.Version,
		"-a", backend,
	}

	if customNginxConf != "" {
		// some parameters are ignored but ports must be published
		args = append(args, "-n", "/etc/nginx/custom/nginx.conf")

		name, err := AddConfig(endpointsPrefix+"custom-", map[string]string{
			"nginx.conf": customNginxConf,
		})

		if err != nil {
			return nil, nil, err
		}

		volumeMounts = append(volumeMounts, api.VolumeMount{
			Name:      name,
			MountPath: "/etc/nginx/custom",
			ReadOnly:  true,
		})
		volumes = append(volumes, makeConfigVolume(name))
	}

	if customAccessLog != "" {
		args = append(args, "--access_log", customAccessLog)
	}

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

		name, err := AddSecret(endpointsPrefix+"ssl-", map[string]string{
			"nginx.key": sslKey,
			"nginx.crt": sslCert,
		})

		if err != nil {
			return nil, nil, err
		}

		volumeMounts = append(volumeMounts, api.VolumeMount{
			Name:      name,
			MountPath: "/etc/nginx/ssl",
			ReadOnly:  true,
		})
		volumes = append(volumes, makeSecretVolume(name))
	}

	args = append(args, "-N", strconv.Itoa(ports.status))
	published = append(published, api.ContainerPort{ContainerPort: int32(ports.status)})

	if cfg.CredentialsFile != "" {
		args = append(args, "-k", "/etc/nginx/creds/secret.json")

		name, err := AddSecret(endpointsPrefix+"creds-", map[string]string{
			"secret.json": cfg.CredentialsFile,
		})

		if err != nil {
			return nil, nil, err
		}

		volumeMounts = append(volumeMounts, api.VolumeMount{
			Name:      name,
			MountPath: "/etc/nginx/creds",
			ReadOnly:  true,
		})
		volumes = append(volumes, makeSecretVolume(name))
	}

	esp := api.Container{
		Name:         MakeName(serviceName),
		Image:        image,
		Ports:        published,
		Args:         args,
		VolumeMounts: volumeMounts,
		LivenessProbe: &api.Probe{
			InitialDelaySeconds: 30,
			TimeoutSeconds:      1,
			Handler: api.Handler{
				HTTPGet: &api.HTTPGetAction{
					Path: "/endpoints_status",
					Port: intstr.FromInt(ports.status),
				},
			},
		},
	}

	return &esp, volumes, nil
}

// CreateDeployment creates ESP pods
func CreateDeployment(app string, ports Ports, backend string) (map[string]string, error) {
	esp, volumes, err := MakeContainer(svc.Name, ports, backend)
	if err != nil {
		return nil, err
	}

	if tight {
		// find deployment by service name
		dpl, err := clientset.Extensions().Deployments(namespace).Get(svc.Name)
		if err != nil {
			return nil, err
		}

		// update deployment
		// TODO garbage collect old secrets
		// TODO document re-deployment
		template := &dpl.Spec.Template
		template.Spec.Containers = append(
			filterContainers(template.Spec.Containers), *esp)
		template.Spec.Volumes = append(
			filterVolumes(template.Spec.Volumes), volumes...)
		template.Labels[ESPManagedService] = svc.Name
		template.Labels[ESPEndpoints] = app

		// commit updates
		_, err = clientset.Extensions().Deployments(namespace).Update(dpl)
		if err != nil {
			return nil, err
		}
		log.Println("Updated deployment", dpl.Name)

		// retrieve selector from the original service
		selector := svc.Spec.Selector
		return selector, nil
	} else {
		replicas := int32(defaultReplicas)
		selector := map[string]string{
			ESPManagedService: svc.Name,
			ESPEndpoints:      app,
		}

		// Create replication controller
		deployment := &extensions.Deployment{
			ObjectMeta: api.ObjectMeta{
				Name: app,
			},
			Spec: extensions.DeploymentSpec{
				Replicas: &replicas,
				Template: api.PodTemplateSpec{
					ObjectMeta: api.ObjectMeta{
						Labels: selector,
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
			return nil, err
		}
		log.Println("Created deployment", deployment.Name)
		return selector, nil
	}
}

// CreateServicePorts lists published ports for ESP container
func CreateServicePorts(ports Ports) []api.ServicePort {
	var published = make([]api.ServicePort, 0)
	v := reflect.ValueOf(ports)
	for i := 0; i < v.NumField(); i++ {
		port := int(v.Field(i).Int())
		if port > 0 {
			published = append(published, api.ServicePort{
				Name:     endpointsPrefix + v.Type().Field(i).Name,
				Protocol: api.ProtocolTCP,
				Port:     int32(port),
			})
		}
	}
	return published
}

// CreateService creates ESP service
func CreateService(
	app string,
	selector map[string]string,
	ports Ports,
	serviceType api.ServiceType) error {
	// publish all ports
	svc := &api.Service{
		ObjectMeta: api.ObjectMeta{
			Name: app,
			Labels: map[string]string{
				ESPManagedService: svc.Name,
			},
		},
		Spec: api.ServiceSpec{
			Ports:    CreateServicePorts(ports),
			Selector: selector,
			Type:     serviceType,
		},
	}

	svc, err := clientset.Core().Services(namespace).Create(svc)
	if err != nil {
		return err
	}
	log.Println("Created service", svc.Name)
	return nil
}

// AddConfig creates a config map from a map of files
func AddConfig(key string, files map[string]string) (string, error) {
	m := map[string]string{}
	for k, fileName := range files {
		data, err := ioutil.ReadFile(fileName)
		if err != nil {
			log.Println("Cannot find config file", fileName)
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

	configMap, err := clientset.Core().ConfigMaps(namespace).Create(config)
	if err != nil {
		return "", err
	}
	log.Printf("Created config map %s from %v\n", configMap.Name, files)
	return configMap.Name, nil
}

// AddSecret creates a secret map from file contents
func AddSecret(key string, files map[string]string) (string, error) {
	m := map[string][]byte{}
	for k, fileName := range files {
		data, err := ioutil.ReadFile(fileName)
		if err != nil {
			log.Println("Cannot find secret file", fileName)
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

	secret, err := clientset.Core().Secrets(namespace).Create(config)
	if err != nil {
		return "", err
	}
	log.Printf("Created secret %s from %v\n", secret.Name, files)
	return secret.Name, nil
}

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

func makeConfigVolume(name string) api.Volume {
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
		if !strings.HasPrefix(v.Name, endpointsPrefix) {
			out = append(out, v)
		}
	}
	return out
}

func filterContainers(containers []api.Container) []api.Container {
	var out = make([]api.Container, 0)
	for _, v := range containers {
		if !strings.HasPrefix(v.Name, endpointsPrefix) {
			out = append(out, v)
		}
	}
	return out
}
