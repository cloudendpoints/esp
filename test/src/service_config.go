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
// ESP CLI
//
package main

import (
	"deploy"
	"fmt"
	"log"
	"os"

	"github.com/spf13/cobra"
)

var (
	cfg           deploy.Service
	serviceAPI    []string
	serviceConfig []string
	serviceProto  []string
)

func check(err error) {
	if err != nil {
		log.Println(err)
		os.Exit(-1)
	}
}

func main() {
	var deployCmd = &cobra.Command{
		Use: "deploy",
		Run: func(cmd *cobra.Command, args []string) {
			check(cfg.Connect())
			out, err := cfg.Deploy(serviceAPI, serviceConfig, serviceProto)
			check(err)
			bytes, err := out.MarshalJSON()
			check(err)
			fmt.Print(string(bytes))
		},
	}

	deployCmd.PersistentFlags().StringArrayVar(&serviceAPI,
		"openapi", nil,
		"OpenAPI specification file(s)")
	deployCmd.PersistentFlags().StringArrayVar(&serviceConfig,
		"config", nil,
		"Service config specification file(s)")
	deployCmd.PersistentFlags().StringArrayVar(&serviceProto,
		"proto", nil,
		"Proto descriptor file(s)")

	var fetchCmd = &cobra.Command{
		Use: "fetch",
		Run: func(cmd *cobra.Command, args []string) {
			check(cfg.Connect())
			out, err := cfg.Fetch()
			check(err)
			report, err := cfg.GenerateConfigReport()
			check(err)
			for _, d := range report.Diagnostics {
				log.Println("[" + d.Kind + "] " +
					d.Location + ": " + d.Message)
			}
			bytes, err := out.MarshalJSON()
			check(err)
			fmt.Print(string(bytes))
		},
	}

	fetchCmd.PersistentFlags().StringVarP(&cfg.Version,
		"version", "v", "",
		"API service config version, empty to use the latest")

	var rootCmd = &cobra.Command{}
	rootCmd.PersistentFlags().StringVarP(&cfg.Name,
		"service", "s", "",
		"API service name")
	rootCmd.PersistentFlags().StringVarP(&cfg.CredentialsFile,
		"creds", "k", "",
		"Service account private key JSON file")
	rootCmd.PersistentFlags().StringVarP(&cfg.ProducerProject,
		"project", "p", "",
		"Service producer project")

	rootCmd.AddCommand(deployCmd, fetchCmd)
	rootCmd.Execute()
}
