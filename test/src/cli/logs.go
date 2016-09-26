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
	"fmt"
	"log"
	"os"

	"golang.org/x/net/context"
	"golang.org/x/oauth2/google"
	logging "google.golang.org/api/logging/v2beta1"

	"github.com/spf13/cobra"

	"k8s.io/client-go/1.5/pkg/api"
	versioned "k8s.io/client-go/1.5/pkg/api/v1"
	"k8s.io/client-go/1.5/pkg/labels"
)

var logCmd = &cobra.Command{
	Use:   "logs [kubernetes service]",
	Short: "Collect ESP logs for a service",
	PreRun: func(cmd *cobra.Command, args []string) {
		if len(args) == 0 {
			log.Println("Please specify kubernetes service name")
			os.Exit(-1)
		}
	},
	Run: func(cmd *cobra.Command, args []string) {
		name := args[0]
		if active {
			ExtractFromPods(name)
		}
		if project != "" {
			ExtractFromGCP(name)
		}
	},
}

var (
	project string
	active  bool
)

func init() {
	RootCmd.AddCommand(logCmd)
	logCmd.PersistentFlags().StringVarP(&project,
		"project", "p", "",
		"GCP project for Kubernetes Google Cloud Logging integration")
	logCmd.PersistentFlags().BoolVarP(&active,
		"active", "a", true,
		"Query kubectl to fetch logs (both stdout and stderr)")
}

func ExtractFromGCP(name string) {
	log.Println("Extracting logs from Google Cloud Logging:")
	ctx := context.Background()
	hc, err := google.DefaultClient(ctx, logging.CloudPlatformScope)
	if err != nil {
		log.Println(err)
		os.Exit(-1)
	}

	client, err := logging.New(hc)
	if err != nil {
		log.Println(err)
		os.Exit(-1)
	}

	// TODO: qualify by cluster_name using resource.labels.cluster_name
	filter := fmt.Sprintf(`resource.type="container"
		AND resource.labels.container_name="%s"
		AND resource.labels.namespace_id="%s"
		AND severity=ERROR`, MakeName(name), namespace)
	log.Println("Filter: ", filter)
	req := &logging.ListLogEntriesRequest{
		OrderBy:    "timestamp asc",
		Filter:     filter,
		ProjectIds: []string{project},
	}

	resp, err := client.Entries.List(req).Fields(
		"entries(resource/labels)",
		"entries(severity,textPayload,timestamp)",
	).Context(ctx).Do()
	if err != nil {
		log.Println(err)
		os.Exit(-1)
	}

	log.Println(len(resp.Entries), "entries available")
	for _, entry := range resp.Entries {
		fmt.Printf("[%s,%s,%s]%s",
			entry.Severity, entry.Timestamp, entry.Resource.Labels["pod_id"],
			entry.TextPayload)
	}
}

// ExtractFromPods fetches logs from pods running ESP container
func ExtractFromPods(name string) {
	log.Println("Extracting logs from existing pods:")
	label := labels.SelectorFromSet(labels.Set(map[string]string{
		ESPManagedService: name,
	}))
	options := api.ListOptions{LabelSelector: label}
	list, err := clientset.Core().Pods(namespace).List(options)
	if err != nil {
		log.Println(err)
		os.Exit(-1)
	}

	var pods = make([]string, len(list.Items))
	for i := 0; i < len(list.Items); i++ {
		pods[i] = list.Items[i].Name
	}

	log.Println(pods)
	for _, pod := range pods {
		PrintLogs(name, pod)
	}
}

// PrintLogs from all pods and containers
func PrintLogs(name, pod string) {
	log.Printf("[pod_id=%s]", pod)
	logOptions := &versioned.PodLogOptions{
		Container: MakeName(name),
	}
	raw, err := clientset.Core().Pods(namespace).GetLogs(pod, logOptions).Do().Raw()
	if err != nil {
		log.Println("Request error", err)
	} else {
		fmt.Println("\n" + string(raw))
	}
}
