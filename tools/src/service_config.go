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
//
////////////////////////////////////////////////////////////////////////////////
//
// Service config CLI
//
package main

import (
	"deploy"
	"fmt"
	"log"
	"os"

	"github.com/spf13/cobra"
)

var s deploy.Service

func check(err error) {
	if err != nil {
		log.Println(err)
		os.Exit(-1)
	}
}

func main() {
	log.SetPrefix("[service_config] ")
	log.SetFlags(0)
	var deploy = &cobra.Command{
		Use: "deploy [config+]",
		Long: `Deploy service config files, which are one or combination of
OpenAPI, Google Service Config, or proto descriptor.
(supported extensions: .yaml, .yml, .json, .pb, .descriptor).`,
		Run: func(cmd *cobra.Command, configs []string) {
			check(s.Connect())
			out, err := s.Deploy(configs, "")
			check(err)
			bytes, err := out.MarshalJSON()
			check(err)
			fmt.Print(string(bytes))
		},
	}
	var fetch = &cobra.Command{
		Use: "fetch",
		Run: func(cmd *cobra.Command, args []string) {
			check(s.Connect())
			out, err := s.Fetch()
			check(err)
			bytes, err := out.MarshalJSON()
			check(err)
			fmt.Print(string(bytes))
		},
	}
	var delete = &cobra.Command{
		Use: "delete",
		Run: func(cmd *cobra.Command, args []string) {
			check(s.Connect())
			check(s.Delete())
		},
	}
	var undelete = &cobra.Command{
		Use: "undelete",
		Run: func(cmd *cobra.Command, args []string) {
			check(s.Connect())
			check(s.Undelete())
		},
	}

	fetch.PersistentFlags().StringVarP(&s.Version,
		"version", "v", "",
		"API service config version, empty to use the latest")

	var root = &cobra.Command{}
	root.PersistentFlags().StringVarP(&s.Name,
		"service", "s", "",
		"API service name")
	root.PersistentFlags().StringVarP(&s.CredentialsFile,
		"creds", "k", "",
		"Service account private key JSON file")
	root.PersistentFlags().StringVarP(&s.ProducerProject,
		"project", "p", "",
		"Service producer project")

	root.AddCommand(deploy, fetch, delete, undelete)
	root.Execute()
}
