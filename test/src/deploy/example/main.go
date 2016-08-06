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
package main

import (
	".."
	"flag"
)

func main() {
	d := deploy.Deployment{
		NginxFile:   "nginx.conf",
		SSLKey:      "nginx.key",
		SSLCert:     "nginx.crt",
		ServiceFile: "service.json",
		K8sYamlFile: "bookstore.yaml",
		K8sService:  "esp-bookstore",
	}

	// Backend
	flag.StringVar(&d.BackendImage, "backend",
		"gcr.io/endpoints-jenkins/bookstore:515b384a27b13138141756f78eea8536ecda9b12",
		"Docker image for the backend")
	flag.StringVar(&d.BackendPort, "backendPort", "8081", "Backend port")

	// ESP
	flag.StringVar(&d.ESPImage, "esp",
		"gcr.io/endpoints-jenkins/endpoints-runtime:debian-git-515b384a27b13138141756f78eea8536ecda9b12",
		"Docker image for ESP")
	flag.StringVar(&d.ESPPort, "p", "8080", "ESP port")
	flag.StringVar(&d.ESPStatusPort, "statusPort", "8090", "ESP status port")
	flag.BoolVar(&d.ESPSsl, "ssl", false, "Enable SSL for ESP port")

	// Service
	flag.StringVar(&d.ServiceName, "api", "testing-dot-endpoints-jenkins.appspot.com", "Service name")
	flag.StringVar(&d.ServiceVersion, "apiVersion", "2016-07-19r1244", "Service version")
	flag.StringVar(&d.ServiceAPIKey, "k", "", "Service API key")
	flag.StringVar(&d.ServiceToken, "s", "", "Service account token file (empty to enable metadata fetching)")

	// Kubernetes
	flag.StringVar(&d.K8sType, "t", "NodePort", `Kubernetes node type (LoadBalancer, NodePort).
	Use LoadBalancer to obtain external IP. Use NodePort for minikube deployment.`)
	flag.StringVar(&d.K8sNamespace, "n", "test", "Kubernetes namespace")

	// Phase
	flag.BoolVar(&d.RunDeploy, "up", false, "Deploy to cluster")
	flag.BoolVar(&d.RunTest, "test", false, "Test on cluster")
	flag.BoolVar(&d.RunTearDown, "down", false, "Tear down the cluster")
	var all bool
	flag.BoolVar(&all, "all", false, "Run all deployment steps")

	flag.Usage = func() {
		flag.PrintDefaults()
	}
	flag.Parse()

	if all {
		d.RunDeploy = true
		d.RunTest = true
		d.RunTearDown = true
	}

	d.Run(func(d *deploy.Deployment) {
		deploy.HTTPGet(d.ESPUrl() + "/shelves?key=" + d.ServiceAPIKey)
	})
}
