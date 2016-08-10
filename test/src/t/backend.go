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
	"strconv"
)

type Backend struct {
	port    int
	address string
}

func startBackend(backendPort int) (*Backend, error) {
	fakeServerPath, err := GetFakeServer()

	be := new(Backend)
	be.port = backendPort
	be.address, err = fakes.StartFakes(strconv.Itoa(backendPort), fakeServerPath)
	if err != nil {
		log.Println("request failed.")
		return nil, err
	}

	return be, nil
}

func (be *Backend) Config(url string, verb string, responseCode int, responseBody string) error {

	config := fakes.Config{
		ConfigID: "",
		Responses: []fakes.ConfigEntry{
			{
				Request: fakes.FakeRequest{
					Url:  url,
					Verb: verb,
				},
				Response: fakes.FakeResponse{
					StatusCode: responseCode,
					Body:       responseBody,
				},
			},
		},
	}

	err := fakes.ConfigureFakesUsingConfig(be.address, config)
	if err != nil {
		log.Println("request failed.")
		return err
	}

	return err
}
