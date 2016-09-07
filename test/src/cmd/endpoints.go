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

package cmd

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"strconv"
	"utils"

	"k8s.io/kubernetes/pkg/api"
	"k8s.io/kubernetes/pkg/labels"

	"github.com/spf13/cobra"
)

var (
	ip string
)

var endpointsCmd = &cobra.Command{
	Use:   "endpoints [kubernetes service]",
	Short: "Describe ESP endpoints for a kubernetes service",
	PreRun: func(cmd *cobra.Command, args []string) {
		if len(args) == 0 {
			fmt.Println("Please specify kubernetes service name")
			os.Exit(-1)
		}
	},
	Run: func(cmd *cobra.Command, args []string) {
		name := args[0]
		out, err := GetESPEndpoints(name)
		if err != nil {
			fmt.Println(err)
			os.Exit(-1)
		}
		bytes, _ := json.MarshalIndent(out, "", "  ")
		fmt.Println(string(bytes))
	},
}

func init() {
	RootCmd.AddCommand(endpointsCmd)
	endpointsCmd.PersistentFlags().StringVar(&ip,
		"ip", "localhost", "Node IP address for NodePort resolution")
}

func GetESPEndpoints(name string) (map[string]map[string]string, error) {
	// collect all services running ESP
	list, err := GetESPServices(name)
	if err != nil {
		return nil, err
	}
	list = append(list, name)

	out := map[string]map[string]string{}

	for _, svc := range list {
		ends, err := GetEndpoints(svc)
		if err != nil {
			fmt.Println(err)
			os.Exit(-1)
		}
		out[svc] = ends
	}

	return out, nil
}

func GetESPServices(name string) ([]string, error) {
	label := labels.SelectorFromSet(labels.Set(map[string]string{
		ESPManagedService: name,
	}))
	options := api.ListOptions{LabelSelector: label}
	list, err := kubectl.Services(namespace).List(options)
	if err != nil {
		return nil, err
	}

	var out = make([]string, len(list.Items))
	for i := 0; i < len(list.Items); i++ {
		out[i] = list.Items[i].Name
	}
	return out, nil
}

// Endpoints retrieves endpoints for a service
func GetEndpoints(name string) (map[string]string, error) {
	svc, err := kubectl.Services(namespace).Get(name)
	if err != nil {
		return nil, err
	}

	out := map[string]string{}

	if svc.Spec.Type == api.ServiceTypeNodePort {
		for _, port := range svc.Spec.Ports {
			out[port.Name] = ip + ":" + strconv.Itoa(int(port.NodePort))
		}
	} else if svc.Spec.Type == api.ServiceTypeLoadBalancer {
		var address string
		ok := utils.Repeat(func() bool {
			fmt.Println("Retrieving address of the service")
			svc, err = kubectl.Services(namespace).Get(name)
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
