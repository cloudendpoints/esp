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
	"fakes"
	"log"
	"net/http"
	"strconv"
)

type MetadataServer struct {
	metadataPort int
	address      string
	metaUrl      string
	tokenUrl     string
}

func startMetadataServer(metadataPort int) (mds *MetadataServer, err error) {

	mds = new(MetadataServer)
	mds.metadataPort = metadataPort

	var fakeServerPath string
	fakeServerPath, err = GetFakeServer()
	if err != nil {
		log.Println("Get fake Metadata server path failed.")
		return nil, err
	}

	mds.address, err = fakes.StartFakes(strconv.Itoa(metadataPort), fakeServerPath)
	if err != nil {
		log.Println("Failed to start Metadata Server.")
		return nil, err
	}

	mds.metaUrl = "/computeMetadata/v1/"

	const metaRes = `
	{
	  "instance": {
	    "attributes": {
	      "gae_app_container": "app",
	      "gae_app_fullname": "esp-test-app_20150921t180445-387321214075436208",
	      "gae_backend_instance": "0",
	      "gae_backend_minor_version": "387321214075436208",
	      "gae_backend_name": "default",
	      "gae_backend_version": "20150921t180445",
	      "gae_project": "esp-test-app",
	      "gae_vm_runtime": "custom",
	      "gcm-pool": "gae-default-20150921t180445",
	      "gcm-replica": "gae-default-20150921t180445-inqp"
	    },
	    "cpuPlatform": "Intel Ivy Bridge",
	    "description": "GAE managed VM for module: default, version: 20150921t180445",
	    "hostname": "gae-default-20150921t180445-inqp.c.esp-test-app.internal",
	    "id": 3296474103533342935,
	    "image": "",
	    "machineType": "projects/345623948572/machineTypes/g1-small",
	    "maintenanceEvent": "NONE",
	    "zone": "projects/345623948572/zones/us-west1-d"
	  },
	  "project": {
	    "numericProjectId": 345623948572,
	    "projectId": "endpoints-app"
	  }
	}
	`

	mds.tokenUrl = "/computeMetadata/v1/instance/service-accounts/default/token"

	const tokenRes = `
	{
		"access_token":"ya29.7gFRTEGmovWacYDnQIpC9X9Qp8cH0sgQyWVrZaB1Eg1WoAhQMSG4L2rtaHk1",
		"expires_in":100,
		"token_type":"Bearer"
	}
	`
	c := fakes.Config{
		ConfigID: "",
		Responses: []fakes.ConfigEntry{
			{
				Request: fakes.FakeRequest{
					Url:  mds.metaUrl,
					Verb: http.MethodGet,
				},
				Response: fakes.FakeResponse{
					StatusCode: http.StatusOK,
					Body:       metaRes,
				},
			},
			{
				Request: fakes.FakeRequest{
					Url:  mds.tokenUrl,
					Verb: http.MethodGet,
				},
				Response: fakes.FakeResponse{
					StatusCode: http.StatusOK,
					Body:       tokenRes,
				},
			},
		},
	}

	err = fakes.ConfigureFakesUsingConfig(mds.address, c)
	if err != nil {
		log.Println("config fake failed.")
	}

	// issue a request for testing
	client := http.Client{}
	res, err := client.Get(mds.address + mds.metaUrl + "?recursive=true")

	if err != nil {
		log.Println("request failed.")
		return nil, err
	}
	if res.StatusCode != http.StatusOK {
		log.Println("Status code is:", res.StatusCode, " expected: ", http.StatusOK)
		return nil, err
	}

	return mds, err
}

func (mds *MetadataServer) GetRequests() (rq fakes.Requests, err error) {
	rq, err = fakes.GetRequests(mds.address, "")

	if err != nil {
		log.Println("Failed to get requests data from metadata server")
	}

	return rq, err
}
