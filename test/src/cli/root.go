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
	"log"
	"os"

	"github.com/spf13/cobra"

	"k8s.io/client-go/1.5/kubernetes"
	api "k8s.io/client-go/1.5/pkg/api/v1"
	"k8s.io/client-go/1.5/tools/clientcmd"
)

var (
	namespace string
	clientset *kubernetes.Clientset
	cfg       deploy.Service
)

// RootCmd for CLI
var RootCmd = &cobra.Command{
	Use:   "espcli",
	Short: "ESP deployment manager for Kubernetes",
	Long:  "A script to deploy ESP and monitor ESP deployments in Kubernetes",
	Run: func(cmd *cobra.Command, args []string) {
		log.Println("Please specify -h option to see the list of options.")
	},
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		config, err := clientcmd.NewNonInteractiveDeferredLoadingClientConfig(
			clientcmd.NewDefaultClientConfigLoadingRules(),
			&clientcmd.ConfigOverrides{},
		).ClientConfig()
		if err != nil {
			log.Println("Cannot find the default Kubernetes configuration.")
			log.Println("Please check with kubectl your cluster config.")
			os.Exit(-2)
		}

		clientset, err = kubernetes.NewForConfig(config)
		if err != nil {
			log.Println("Cannot connect to the Kubernetes API: ", err)
			os.Exit(-2)
		}
	},
}

func init() {
	RootCmd.PersistentFlags().StringVar(&namespace,
		"namespace", api.NamespaceDefault, "Specify Kubernetes namespace")
}
