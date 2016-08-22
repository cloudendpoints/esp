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
	"errors"
	"log"
	"net/http"
	"strconv"
	"strings"
	"time"

	"fakes"
)

type ServiceController struct {
	serviceName        string
	serviceControlPort int
	address            string
	checkUrl           string
	reportUrl          string
}

func startServiceController(serviceName string, serviceControlPort int) (sc *ServiceController, err error) {

	sc = new(ServiceController)
	sc.serviceName = serviceName
	sc.serviceControlPort = serviceControlPort

	var fakeServerPath string
	fakeServerPath, err = GetFakeServer()
	if err != nil {
		log.Println("Get fake Metadata server path failed.")
		return nil, err
	}

	sc.checkUrl = "/v1/services/" + serviceName + ":check"
	sc.reportUrl = "/v1/services/" + serviceName + ":report"

	sc.address, err = fakes.StartFakes(strconv.Itoa(serviceControlPort), fakeServerPath)
	if err != nil {
		log.Println("request failed.")
		return nil, err
	}

	return sc, err
}

func (sc *ServiceController) Config(checkRespCode int, checkRespBody string, reportRespCode int, reportRespBody string) error {

	config := fakes.Config{
		ConfigID: "",
		Responses: []fakes.ConfigEntry{
			{
				Request: fakes.FakeRequest{
					Url:  sc.checkUrl,
					Verb: http.MethodPost,
				},
				Response: fakes.FakeResponse{
					StatusCode: checkRespCode,
					Body:       checkRespBody,
				},
			},
			{
				Request: fakes.FakeRequest{
					Url:  sc.reportUrl,
					Verb: http.MethodPost,
				},
				Response: fakes.FakeResponse{
					StatusCode: reportRespCode,
					Body:       reportRespBody,
				},
			},
		},
	}

	err := fakes.ConfigureFakesUsingConfig(sc.address, config)
	if err != nil {
		log.Println("request failed.")
		return err
	}

	client := http.Client{}
	res, err := client.Post(sc.address+sc.checkUrl, "application/json", strings.NewReader(""))

	if err != nil {
		log.Println("request failed.")
		return err
	}
	if res.StatusCode != http.StatusOK {
		log.Println("Not 200.")
		return err
	}

	return err
}

func GetServiceControlData(server string, n, timeout int) (rq fakes.Requests, err error) {
	// Wait until timeout seconds to wait for n requests.
	// Report is cached by ApiManager for 1 second. If the caller needs to get Report
	// data, it may need to wait at least for 1 second.
	for i := 0; i < timeout*10; i++ {
		rq, err := fakes.GetRequests(server, "")
		if err != nil {
			log.Println("failed to get data from service control server")
			return rq, err
		}
		if len(rq) >= n {
			return rq, nil
		}
		time.Sleep(100 * time.Millisecond)
	}
	if len(rq) < n {
		log.Println("timed out while waiting for service control data")
		return rq, errors.New("timed out while waiting for service control data")
	}
	return rq, nil
}
