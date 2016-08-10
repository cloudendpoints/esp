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
	"fmt"
	"log"
	"strconv"
	"utils"
)

type EspInstance struct {
	sandboxDir            string
	serviceConfigFileName string
	nginxPort             int
	backendPort           int
}

func GetNginxBinary() (string, error) {
	test_bin_root, err := utils.GetTestBinRootPath()
	if err != nil {
		return "", err
	}
	path := test_bin_root + "/src/nginx/main/nginx-esp"
	fmt.Println("Nginx server path", path)
	return path, nil
}

func startEspInstance(
	sandboxParentDir string,
	serviceConfigFileName string,
	nginxPort int,
	backendPort int,
	metadataPort int,
	pubKeyPort int,
	authenticated bool,
) (*EspInstance, error) {

	var err error

	esp := new(EspInstance)
	esp.serviceConfigFileName = serviceConfigFileName
	esp.nginxPort = nginxPort
	esp.backendPort = backendPort

	sandboxDir, err := deploy.CreateSandbox(sandboxParentDir)
	if err != nil {
		log.Println("CreateSandbox failed.")
		return nil, err
	}
	esp.sandboxDir = sandboxDir

	var metadataServer = "http://127.0.0.1:" + strconv.Itoa(metadataPort)
	nginxConf, err := utils.NginxConfig(nginxPort, backendPort, serviceConfigFileName, sandboxDir, metadataServer)
	if err != nil {
		log.Println("Get Nginx Config failed.")
		return nil, err
	}

	var serviceControlUrl = "http://127.0.0.1:" + strconv.Itoa(serviceControlPort)
	var serviceConf string

	if authenticated {
		providers := []utils.AuthProvider{
			{
				Issuer:    "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com",
				KeyUrl:    "http://127.0.0.1:" + strconv.Itoa(pubKeyPort) + "/pubkey",
				Audiences: "ok_audience_1,ok_audience_2",
			},
		}
		serviceConf, err = utils.ServiceConfig(serviceName, serviceControlUrl, providers)
	} else {
		serviceConf, err = utils.ServiceConfig(serviceName, serviceControlUrl, []utils.AuthProvider{})
	}

	if err != nil {
		log.Println("Get Service Config failed.")
		return nil, err
	}

	err = deploy.PrepareSandbox(sandboxDir, nginxConf, serviceConf)
	if err != nil {
		return nil, err
	}

	var nginxBinary string
	nginxBinary, err = GetNginxBinary()
	if err != nil {
		log.Println("Cannot get Nginx binary.")
		return nil, err
	}

	_, err = deploy.StartEsp(nginxBinary, sandboxDir, 5)
	if err != nil {
		log.Println("StartEsp failed.")
		return nil, err
	}

	return esp, err
}
