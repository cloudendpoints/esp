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
	"os"

	"github.com/spf13/cobra"

	"k8s.io/client-go/1.4/kubernetes"
	"k8s.io/client-go/1.4/rest"
)

var (
	namespace string
	control   int
	clientset *kubernetes.Clientset
)

// Prefix for ESP managed resources
const endpoints = "endpoints"

// Label to tag pods running ESP for a given k8s service
// Label to tag services points to ESP pods for a given k8s service
const ESPManagedService = "googleapis.com/service"

// Label to tag pods with loosely coupled ESP services
// (to support multiple ESP deployments for a service)
const ESPEndpoints = "googleapis.com/endpoints"

// API Service name label on kubernetes servies
const ESPConfig = "googleapis.com/api"

// RootCmd for CLI
var RootCmd = &cobra.Command{
	Use:   "espcli",
	Short: "ESP deployment manager for Kubernetes",
	Long:  "A script to deploy ESP and monitor ESP deployments in Kubernetes",
	Run: func(cmd *cobra.Command, args []string) {
		fmt.Println("ESP deployment command line interface")
	},
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		config := rest.Config{
			Host: fmt.Sprintf("localhost:%d", control),
		}

		var err error
		clientset, err = kubernetes.NewForConfig(&config)
		if err != nil {
			fmt.Println("Cannot connect to Kubernetes API: ", err)
			os.Exit(-2)
		}
	},
}

func init() {
	RootCmd.PersistentFlags().StringVar(&namespace,
		"namespace", "default", "kubernetes namespace")
	RootCmd.PersistentFlags().IntVar(&control,
		"control", 9000, "kubectl proxy port")
}
