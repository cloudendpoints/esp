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
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"os"
	"strconv"
	"utils"

	"github.com/spf13/cobra"

	"k8s.io/client-go/1.5/pkg/api"
	versioned "k8s.io/client-go/1.5/pkg/api/v1"
	"k8s.io/client-go/1.5/pkg/labels"
)

var endpointsCmd = &cobra.Command{
	Use:   "endpoints [Kubernetes service]",
	Short: "Describe ESP endpoints for a Kubernetes service",
	PreRun: func(cmd *cobra.Command, args []string) {
		if len(args) == 0 {
			log.Println("Please specify Kubernetes service name")
			os.Exit(-1)
		}
	},
	Run: func(cmd *cobra.Command, args []string) {
		name := args[0]
		out, err := GetESPEndpoints(name)
		if err != nil {
			log.Println(err)
			os.Exit(-1)
		}
		bytes, _ := json.MarshalIndent(out, "", "  ")
		fmt.Println(string(bytes))
	},
}

func init() {
	RootCmd.AddCommand(endpointsCmd)
}

// GetESPEndpoints collects endpoints information
func GetESPEndpoints(name string) (map[string]map[string]string, error) {
	// collect all services running ESP
	list, err := GetESPServices(name)
	if err != nil {
		return nil, err
	}

	out := map[string]map[string]string{}

	for _, svc := range list {
		ends, err := GetEndpoints(svc)
		if err != nil {
			log.Println(err)
			os.Exit(-1)
		}
		out[svc.Name] = ends
	}

	return out, nil
}

// GetESPServices for a Kubernetes service
func GetESPServices(name string) ([]*versioned.Service, error) {
	label := labels.SelectorFromSet(labels.Set(map[string]string{
		AnnotationManagedService: name,
	}))
	options := api.ListOptions{LabelSelector: label}
	list, err := clientset.Core().Services(namespace).List(options)
	if err != nil {
		return nil, err
	}

	var out = make([]*versioned.Service, len(list.Items))
	for i := 0; i < len(list.Items); i++ {
		out[i] = &list.Items[i]
	}
	return out, nil
}

// GetEndpoints retrieves endpoints information for an ESP service
func GetEndpoints(svc *versioned.Service) (map[string]string, error) {
	out := map[string]string{}

	out[AnnotationConfigId] = svc.Annotations[AnnotationConfigId]
	out[AnnotationConfigName] = svc.Annotations[AnnotationConfigName]
	out[AnnotationDeploymentType] = svc.Annotations[AnnotationDeploymentType]

	if svc.Spec.Type == versioned.ServiceTypeNodePort {
		for _, port := range svc.Spec.Ports {
			out[port.Name] = "NODE_IP:" + strconv.Itoa(int(port.NodePort))
		}
	} else if svc.Spec.Type == versioned.ServiceTypeClusterIP {
		for _, port := range svc.Spec.Ports {
			out[port.Name] = svc.Name + ":" + strconv.Itoa(int(port.Port))
		}
	} else if svc.Spec.Type == versioned.ServiceTypeLoadBalancer {
		var address string
		var err error
		ok := utils.Repeat(func() bool {
			log.Println("Retrieving address of the service " + svc.Name)
			svc, err = clientset.Core().Services(namespace).Get(svc.Name)
			if err != nil {
				return false
			}

			ingress := svc.Status.LoadBalancer.Ingress
			if len(ingress) == 0 {
				return false
			}

			if ingress[0].IP != "" {
				address = ingress[0].IP
				return true
			} else if ingress[0].Hostname != "" {
				address = ingress[0].Hostname
				return true
			}

			return false
		})

		if !ok {
			return nil, errors.New("Failed to retrieve IP of the service")
		}

		for _, port := range svc.Spec.Ports {
			out[port.Name] = address + ":" + strconv.Itoa(int(port.Port))
		}
	} else {
		return nil, errors.New("Cannot handle service type")
	}

	return out, nil
}
