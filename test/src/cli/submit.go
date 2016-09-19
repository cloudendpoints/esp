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
)

var submitCmd = &cobra.Command{
	Use:   "submit",
	Short: "Submit service configuration files. Creates an API service if necessary.",
	Run: func(cmd *cobra.Command, args []string) {
		files, err := deploy.MakeConfigFiles(serviceAPI, serviceConfig, serviceProto)
		if err != nil {
			log.Println(err)
			os.Exit(-1)
		}

		err = cfg.Init()
		if err != nil {
			log.Println(err)
			os.Exit(-1)
		}

		if !cfg.Exists() {
			err = cfg.Create()
			if err != nil {
				log.Println(err)
				os.Exit(-1)
			}
		}

		_, err = cfg.Submit(files)
		if err != nil {
			log.Println(err)
			os.Exit(-1)
		}

		err = cfg.Enable(cfg.ProducerProject)
		if err != nil {
			log.Println(err)
			os.Exit(-1)
		}

		err = cfg.Rollout()
		if err != nil {
			log.Println(err)
			os.Exit(-1)
		}

		log.Println("Finished service config submission")
	},
}

var (
	serviceAPI    []string
	serviceConfig []string
	serviceProto  []string
)

func init() {
	RootCmd.AddCommand(submitCmd)
	submitCmd.PersistentFlags().StringArrayVar(&serviceAPI,
		"openapi", nil,
		"OpenAPI specification file(s)")
	submitCmd.PersistentFlags().StringArrayVar(&serviceConfig,
		"config", nil,
		"Service config specification file(s)")
	submitCmd.PersistentFlags().StringArrayVar(&serviceProto,
		"proto", nil,
		"Proto descriptor file(s)")
	submitCmd.PersistentFlags().StringVarP(&cfg.Name,
		"service", "s", "",
		"Service API name")
	submitCmd.PersistentFlags().StringVarP(&cfg.ProducerProject,
		"project", "p", "",
		"Producer project ID")
	submitCmd.PersistentFlags().StringVarP(&cfg.CredentialsFile,
		"creds", "k", "",
		"Service account private key JSON file")
}
