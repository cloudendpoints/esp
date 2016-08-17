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
// Integration test driver
//
package main

import (
	"deploy"
	"flag"
	"log"
	"os"
	"t"
	"utils"
)

var (
	// Where to deploy (GKE, GCE, ...)
	engine string

	// Deployment: bring up, run test, tear down
	up   bool
	test bool
	down bool

	// Application and service
	esp     deploy.Application
	backend deploy.Application
	service deploy.Service

	// Optional deployment options
	namespace  string
	ctlport    int
	minikubeIP string
)

const (
	gke = "gke"
)

func init() {
	flag.StringVar(&engine, "e", gke, "Select integration test")

	// Phase control (set at least one or all)
	flag.BoolVar(&up, "up", false, "Deploy only")
	flag.BoolVar(&test, "test", false, "Test only")
	flag.BoolVar(&down, "down", false, "Tear down only")

	// Service
	flag.StringVar(&service.Name, "service",
		"testing-dot-endpoints-jenkins.appspot.com",
		"Service name")
	flag.StringVar(&service.Version, "version", "",
		"Service version")
	flag.StringVar(&service.Key, "k", "",
		"Service API key")
	flag.StringVar(&service.Token, "cred", "",
		"Service account token file (optional)")

	// Application image
	flag.StringVar(&backend.Image, "backend",
		"gcr.io/endpoints-jenkins/bookstore:fdec9b855c0b368a78c086bee4b77247f9e2d55e",
		"Docker image for the backend")
	flag.IntVar(&backend.Port, "backendPort", 8081, "Backend port")
	backend.Name = "bookstore"
	backend.Status = "/shelves"

	// ESP image
	flag.StringVar(&esp.Image, "esp",
		"gcr.io/endpoints-jenkins/esp-autoconf2",
		"Docker image for ESP")
	flag.IntVar(&esp.Port, "port", 8080, "ESP port")
	flag.IntVar(&esp.StatusPort, "status", 8090, "ESP status port")
	flag.IntVar(&esp.SSLPort, "ssl", 0, "Enable SSL for ESP using this port (0 to disable)")
	esp.Name = "esp"
	esp.Status = "/endpoints_status"

	// Kubernetes
	flag.StringVar(&namespace, "namespace", "", "Kubernetes namespace (empty to generate)")
	flag.IntVar(&ctlport, "kubectl", 9000, "kubectl proxy port")
	flag.StringVar(&minikubeIP, "ip", "", "Use NodePort with the given host IP (minikube specific)")
}

func parse() {
	flag.Usage = func() {
		flag.PrintDefaults()
	}

	flag.Parse()

	// By default, run all phases
	if !up && !test && !down {
		up = true
		test = true
		down = true
	}

	// Use same status port for backend as actual port
	backend.StatusPort = backend.Port

}

func main() {
	parse()

	switch engine {
	case gke:
		kubernetes()
	}
}

func kubernetes() {
	d := deploy.Deployment{
		ESP:     &esp,
		Backend: &backend,
		Config:  &service,
		SSLKey:  utils.SSLKeyFile(),
		SSLCert: utils.SSLCertFile(),
		IP:      minikubeIP,
	}

	d.Init(ctlport, namespace)

	// deploy to cluster
	if up {
		// Fetch service version if necessary
		if service.Version == "" {
			service.Version, _ = service.GetVersion()
		}
		if !d.Deploy() {
			log.Fatalln("Failed to deploy")
		}
	}

	var result = true
	// run test
	if test {
		addr := d.WaitReady()
		result = t.RunBookstore(addr, service)
	}

	// tear down the cluster
	if down {
		d.CollectLogs()
		if !d.TearDown() {
			log.Fatalln("Failed to tear down")
		}
	}

	if !result {
		log.Println("Test failed")
		os.Exit(1)
	} else {
		log.Println("Test passed")
	}
}
