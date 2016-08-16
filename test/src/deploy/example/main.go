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
	"log"
	"os/exec"
	"regexp"
	"strings"
)

// Root of the repository
const root = "../../../.."

func main() {
	d := deploy.Deployment{
		ESP: deploy.KubernetesService{
			Name:   "esp",
			Status: "/endpoints_status",
		},
		Backend: deploy.KubernetesService{
			Name:   "backend",
			Status: "/shelves",
		},
		NginxFile: "nginx.conf",
		SSLKey:    "nginx.key",
		SSLCert:   "nginx.crt",
	}

	// Backend
	flag.StringVar(&d.Backend.Image, "backend",
		"gcr.io/endpoints-jenkins/bookstore:fdec9b855c0b368a78c086bee4b77247f9e2d55e",
		"Docker image for the backend")
	flag.IntVar(&d.Backend.Port, "backendPort", 8081, "Backend port")

	// ESP
	flag.StringVar(&d.ESP.Image, "esp",
		"gcr.io/endpoints-jenkins/esp-autoconf2",
		"Docker image for ESP")
	flag.IntVar(&d.ESP.Port, "port", 8080, "ESP port")
	flag.IntVar(&d.ESP.StatusPort, "status", 8090, "ESP status port")
	flag.IntVar(&d.ESP.SSLPort, "ssl", 0, "Enable SSL for ESP using this port")

	// Service
	flag.StringVar(&d.ServiceName, "service", "testing-dot-endpoints-jenkins.appspot.com", "Service name")
	flag.StringVar(&d.ServiceVersion, "version", "2016-07-19r1244", "Service version")

	flag.StringVar(&d.ServiceAPIKey, "k", "", "Service API key")
	flag.StringVar(&d.ServiceToken, "s", "", "Service account token file")

	// Kubernetes
	flag.StringVar(&d.K8sNamespace, "namespace", "test", "Kubernetes namespace")
	flag.IntVar(&d.K8sPort, "p", 9000, "kubectl proxy port")
	flag.StringVar(&d.Minikube, "ip", "", "Use NodePort with the given host IP (minikube specific)")

	// Phase
	var all bool
	flag.BoolVar(&all, "all", false, "Run all deployment steps")
	flag.BoolVar(&d.RunDeploy, "up", false, "Deploy to cluster")
	flag.BoolVar(&d.RunTest, "test", false, "Test on cluster")
	flag.BoolVar(&d.RunTearDown, "down", false, "Tear down the cluster")

	flag.Usage = func() {
		flag.PrintDefaults()
	}
	flag.Parse()

	// Use same status port for backend as actual port
	d.Backend.StatusPort = d.Backend.Port

	if all {
		d.RunDeploy = true
		d.RunTest = true
		d.RunTearDown = true
	}

	testToken := GetAuthToken(root+"/client/custom/esp-test-client-secret-jwk.json", d.ServiceName)

	d.Run(func(url string) {

		// Run test
		out, err := Run(root+"/test/client/esp_bookstore_test.py",
			"--verbose=true",
			"--host="+url,
			"--api_key="+d.ServiceAPIKey,
			"--auth_token="+testToken,
			"--allow_unverified_cert=true")
		log.Println(out)

		if err != nil {
			log.Println("Test failed: ", err)
		}
	})
}

func GetAuthToken(token, audience string) string {
	// Make an auth token
	out, err := Run(root+"/bazel-bin/src/tools/auth_token_gen",
		token, audience)
	if err != nil {
		log.Fatalln("Cannot generate auth token")
	}

	outputStr := string(out)
	re := regexp.MustCompile("Auth token:?")
	loc := re.FindStringIndex(outputStr)
	jwt := outputStr[loc[1]+1 : len(outputStr)-1]

	return jwt
}

// Execute command and return the error code, merged output from stderr and stdout
func Run(name string, args ...string) (s string, err error) {
	log.Println(">", name, strings.Join(args, " "))
	c := exec.Command(name, args...)
	bytes, err := c.Output()
	if err != nil {
		log.Println(err)
	}
	s = string(bytes)
	return
}
