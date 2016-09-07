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
	"fmt"
	"os"

	"github.com/spf13/cobra"
	"k8s.io/kubernetes/pkg/api"
	"k8s.io/kubernetes/pkg/labels"
)

func init() {
	RootCmd.AddCommand(logCmd)
}

var logCmd = &cobra.Command{
	Use:   "logs [kubernetes service]",
	Short: "Collect ESP logs for a service",
	PreRun: func(cmd *cobra.Command, args []string) {
		if len(args) == 0 {
			fmt.Println("Please specify kubernetes service name")
			os.Exit(-1)
		}
	},
	Run: func(cmd *cobra.Command, args []string) {
		name := args[0]
		pods := GetESPPods(name)
		fmt.Println("Extracting logs from pods", pods)
		for _, pod := range pods {
			PrintLogs(pod)
		}
	},
}

// Pods running ESP managing services
func GetESPPods(name string) []string {
	label := labels.SelectorFromSet(labels.Set(map[string]string{
		ESPManagedService: name,
	}))
	options := api.ListOptions{LabelSelector: label}
	list, err := kubectl.Pods(namespace).List(options)
	if err != nil {
		return nil
	}

	var out = make([]string, len(list.Items))
	for i := 0; i < len(list.Items); i++ {
		out[i] = list.Items[i].Name
	}
	return out
}

// PrintLogs from all pods and containers
func PrintLogs(pod string) {
	fmt.Printf("Logs for %s in %s", endpoints, pod)
	logOptions := &api.PodLogOptions{
		Container: endpoints,
	}
	raw, err := kubectl.Pods(namespace).GetLogs(pod, logOptions).Do().Raw()
	if err != nil {
		fmt.Println("Request error", err)
	} else {
		fmt.Println("\n" + string(raw))
	}
}
