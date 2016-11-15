// Copyright (C) Extensible Service Proxy Authors
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
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"reflect"
	"strconv"
	"strings"

	"github.com/spf13/cobra"

	base "k8s.io/client-go/1.5/pkg/api"
	api "k8s.io/client-go/1.5/pkg/api/v1"
	extensions "k8s.io/client-go/1.5/pkg/apis/extensions/v1beta1"
	"k8s.io/client-go/1.5/pkg/util/intstr"
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

	svc *api.Service

	configs []string

	grpc bool
	// loose or tight
	containerType string
)

// Default number of replicas for ESP service
const defaultReplicas = 1

func check(msg string, err error) {
	if err != nil {
		log.Println(msg, err)
		os.Exit(-1)
	}
}

var deployCmd = &cobra.Command{
	Use:   "deploy [source Kubernetes service] [target ESP service]",
	Short: "Deploy ESP for an existing Kubernetes service",
	Run: func(cmd *cobra.Command, args []string) {
		// Must take one parameter
		if len(args) != 2 {
			log.Println("Please specify Kubernetes service/deployment name and ESP service name")
			os.Exit(-1)
		}

		name := args[0]
		app := args[1]

		// First argument is the service name
		var err error
		svc, err = clientset.Core().Services(namespace).Get(name)
		check("Cannot find Kubernetes service:", err)
		err = CreateServiceConfig(name, namespace)
		check("Failed to create service config:", err)
		backend, err := GetBackend()
		check("Failed to retrieve application backend address:", err)
		selector, err := InjectDeployment(app, ports, backend)
		check("Failed to inject deployment:", err)
		err = InjectService(app, selector, ports, api.ServiceType(serviceType))
		check("Failed to inject service:", err)
	},
}

func init() {
	RootCmd.AddCommand(deployCmd)
	deployCmd.PersistentFlags().StringVar(&image,
		"image",
		"b.gcr.io/endpoints/endpoints-runtime:latest",
		"URL to ESP docker image")

	deployCmd.PersistentFlags().StringVarP(&cfg.Name,
		"service", "s", "",
		"API service name")
	deployCmd.PersistentFlags().StringVarP(&cfg.Version,
		"version", "v", "",
		"API service config ID, empty to use the latest")
	deployCmd.PersistentFlags().StringVarP(&cfg.CredentialsFile,
		"creds", "k", "",
		"Service account credentials JSON file")
	deployCmd.PersistentFlags().StringVar(&cfg.ProducerProject,
		"project", "",
		"Service producer project (optional if you use service account credentials)")

	deployCmd.PersistentFlags().IntVarP(&ports.status,
		"status", "N", 8090, "Status port for ESP (always enabled)")
	deployCmd.PersistentFlags().IntVarP(&ports.http,
		"http", "p", 80, "HTTP/1.x port to use for ESP")
	deployCmd.PersistentFlags().IntVarP(&ports.http2,
		"http2", "P", 0, "HTTP/2 port to use for ESP")
	deployCmd.PersistentFlags().IntVarP(&ports.ssl,
		"ssl", "S", 0, "HTTPS port to use for ESP")
	deployCmd.PersistentFlags().StringVarP(&customNginxConf,
		"nginx_config", "n", "", "Use a custom nginx config file")
	deployCmd.PersistentFlags().StringVar(&customAccessLog,
		"access_log", "", "Use a custom nginx access log file (or 'off' to disable)")

	deployCmd.PersistentFlags().StringVar(&sslKey,
		"sslKey", "", "SSL key file")
	deployCmd.PersistentFlags().StringVar(&sslCert,
		"sslCert", "", "SSL certificate file")

	deployCmd.PersistentFlags().BoolVarP(&grpc,
		"grpc", "g", false,
		"Use GRPC to communicate with the backend")
	deployCmd.PersistentFlags().StringVarP(&containerType,
		"deploy", "d", "tight",
		"Specify deployment (tight or loose)")
	deployCmd.PersistentFlags().StringVarP(&serviceType,
		"serviceType", "e", string(api.ServiceTypeClusterIP),
		fmt.Sprintf("Expose ESP service as the provided service type (one of %s, %s, %s)",
			api.ServiceTypeClusterIP, api.ServiceTypeNodePort, api.ServiceTypeLoadBalancer))

	deployCmd.PersistentFlags().StringArrayVarP(&configs,
		"config", "c", nil, "Service config file, one or combination of "+
			"OpenAPI, Google Service Config, or proto descriptor. "+
			"(supported extensions: .yaml, .yml, .json, .pb, .descriptor).")
}

// CreateServiceConfig creates/enables/submits configuration for the service
func CreateServiceConfig(name, namespace string) error {
	err := cfg.Connect()
	if err != nil {
		return err
	}

	defaultName := name + "." + namespace + "." + cfg.ProducerProject + ".appspot.com"

	if cfg.Version != "" {
		// retrieve config
		if len(configs) > 0 {
			return errors.New("Cannot submit config sources for a specific version")
		}
		if cfg.Name == "" {
			cfg.Name = defaultName
		}
	} else {
		// submit config
		_, err := cfg.Deploy(configs, defaultName)
		if err != nil {
			return err
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
	if containerType == "tight" {
		if svcPort.TargetPort.Type != intstr.Int {
			return "", errors.New("Service target port should not be a named port")
		}
		port := int(svcPort.TargetPort.IntVal)
		backend = "127.0.0.1:" + strconv.Itoa(port)
	} else {
		backend = svc.Name + ":" + strconv.Itoa(int(svcPort.Port))
	}

	if grpc {
		return "grpc://" + backend, nil
	}

	return backend, nil
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

		name, err := AddConfig(EndpointsPrefix+"custom-", map[string]string{
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

		name, err := AddSecret(EndpointsPrefix+"ssl-", map[string]string{
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

		name, err := AddSecret(EndpointsPrefix+"creds-", map[string]string{
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
		Name:         EndpointsPrefix + serviceName,
		Image:        image,
		Ports:        published,
		Args:         args,
		VolumeMounts: volumeMounts,
		ReadinessProbe: &api.Probe{
			Handler: api.Handler{
				HTTPGet: &api.HTTPGetAction{
					Path: "/healthz",
					Port: intstr.FromInt(ports.status),
				},
			},
		},
	}

	return &esp, volumes, nil
}

// TODO garbage collect old secrets
// TODO document re-deployment
// TODO leaking deployment when switching to tight

// InjectDeployment creates ESP pods
func InjectDeployment(app string, ports Ports, backend string) (map[string]string, error) {
	var deployment *extensions.Deployment
	var selector map[string]string

	esp, volumes, err := MakeContainer(svc.Name, ports, backend)
	if err != nil {
		return nil, err
	}

	if containerType == "tight" {
		// retrieve selector from the original service
		selector = svc.Spec.Selector

		// find deployment by service name
		deployment, err = clientset.Extensions().Deployments(namespace).Get(svc.Name)
		if err != nil {
			return nil, err
		}

		// update deployment
		template := &deployment.Spec.Template
		template.Spec.Containers = append(
			filterContainers(template.Spec.Containers), *esp)
		template.Spec.Volumes = append(
			filterVolumes(template.Spec.Volumes), volumes...)
		template.Labels[AnnotationManagedService] = svc.Name
		template.Labels[AnnotationEndpointsService] = app

		// commit updates
		_, err = clientset.Extensions().Deployments(namespace).Update(deployment)
		if err != nil {
			return nil, err
		}
		log.Println("Updated deployment", deployment.Name)
	} else {
		replicas := int32(defaultReplicas)
		selector = map[string]string{
			AnnotationManagedService:   svc.Name,
			AnnotationEndpointsService: app,
		}

		// Create replication controller
		deployment = &extensions.Deployment{
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

		// check if it exists
		_, err = clientset.Extensions().Deployments(namespace).Get(app)
		if err == nil {
			deployment, err = clientset.Extensions().Deployments(namespace).Update(deployment)
			if err != nil {
				return nil, err
			}
			log.Println("Updated deployment", deployment.Name)
		} else {
			deployment, err = clientset.Extensions().Deployments(namespace).Create(deployment)
			if err != nil {
				return nil, err
			}
			log.Println("Created deployment", deployment.Name)

		}
	}
	return selector, nil
}

// CreateServicePorts lists published ports for ESP container
func CreateServicePorts(ports Ports) []api.ServicePort {
	var published = make([]api.ServicePort, 0)
	v := reflect.ValueOf(ports)
	for i := 0; i < v.NumField(); i++ {
		port := int(v.Field(i).Int())
		if port > 0 {
			published = append(published, api.ServicePort{
				Name:     EndpointsPrefix + v.Type().Field(i).Name,
				Protocol: api.ProtocolTCP,
				Port:     int32(port),
			})
		}
	}
	return published
}

// InjectService creates ESP service
func InjectService(
	app string,
	selector map[string]string,
	ports Ports,
	serviceType api.ServiceType) (err error) {
	// publish all ports
	proxyService := &api.Service{
		ObjectMeta: api.ObjectMeta{
			Name: app,
			Labels: map[string]string{
				AnnotationManagedService: svc.Name,
			},
			Annotations: map[string]string{
				AnnotationConfigName:     cfg.Name,
				AnnotationConfigId:       cfg.Version,
				AnnotationDeploymentType: containerType,
			},
		},
		Spec: api.ServiceSpec{
			Ports:    CreateServicePorts(ports),
			Selector: selector,
			Type:     serviceType,
		},
	}

	_, err = clientset.Core().Services(namespace).Get(app)
	if err == nil {
		log.Printf("Service %s already exists\n", app)
		err = clientset.Core().Services(namespace).Delete(app, &base.DeleteOptions{})
		if err != nil {
			return
		}
		log.Println("Deleted service", app)
	}

	_, err = clientset.Core().Services(namespace).Create(proxyService)
	if err != nil {
		return
	}
	log.Println("Created service", app)

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
		if !strings.HasPrefix(v.Name, EndpointsPrefix) {
			out = append(out, v)
		}
	}
	return out
}

func filterContainers(containers []api.Container) []api.Container {
	var out = make([]api.Container, 0)
	for _, v := range containers {
		if !strings.HasPrefix(v.Name, EndpointsPrefix) {
			out = append(out, v)
		}
	}
	return out
}
