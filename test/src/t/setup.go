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
package t

import (
	"deploy/deploy-local"
	"fakes"
	"fmt"
	"log"
	"utils"
)

func GetFakeServer() (string, error) {
	// Requires first to build fakes_server binary
	// 1. go get ./...
	// 2. go build -o bazel-bin/test/src/fakes/fakes_server fakes/server/main.go
	test_bin_root, err := utils.GetTestBinRootPath()
	if err != nil {
		return "", err
	}
	path := test_bin_root + "/test/src/fakes_server"
	fmt.Println("Fake server path", path)
	return path, nil
}

type TestSetup struct {
	serviceController *ServiceController
	backend           *Backend
	esp               *EspInstance
	publicKeyServer   *PublicKeyServer
	metadataServer    *MetadataServer
}

func SetupLocal(
	backendPort int,
	nginxPort int,
	serviceControlPort int,
	metadataPort int,
	pubKeyServerPort int,
	serviceName string,
	sandboxParentDir string,
	serviceConfigFileName string,
	authenticationOn bool,
) (ts TestSetup, err error) {

	// start backend
	ts.backend, err = startBackend(backendPort)
	if err != nil {
		log.Println("Fake backend failed.")
	}

	// start service control
	ts.serviceController, err = startServiceController(serviceName, serviceControlPort)
	if err != nil {
		log.Println("Fake service control failed.")
	}

	// start metadata server
	ts.metadataServer, err = startMetadataServer(metadataPort)
	if err != nil {
		log.Println("Fake metadata server failed.")
	}

	// start public key server
	ts.publicKeyServer, err = startPublicKeyServer(pubKeyServerPort)
	if err != nil {
		log.Println("Fake public key server failed.")
	}

	// deploy ESP locally
	ts.esp, err = startEspInstance(
		sandboxParentDir,
		serviceConfigFileName,
		nginxPort,
		backendPort,
		metadataPort,
		pubKeyServerPort,
		authenticationOn,
	)
	if err != nil {
		log.Println("Starting ESP failed.")
	}

	return ts, err
}

func TearDownLocal(ts TestSetup, saveSandbox bool) {

	// stop service control
	fakes.StopFakes(ts.backend.address)

	// stop backend
	fakes.StopFakes(ts.serviceController.address)

	// stop metadata server
	fakes.StopFakes(ts.metadataServer.address)

	// stop public key server
	fakes.StopFakes(ts.publicKeyServer.address)

	// stop ESP
	deploy.StopEsp(ts.esp.sandboxDir, !saveSandbox, 5)

}
