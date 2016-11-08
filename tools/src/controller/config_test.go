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

package testing

import (
	"io/ioutil"
	"os"
	"testing"

	"controller"
	"utils"
)

func check(t *testing.T, err error) {
	if err != nil {
		t.Error(err)
	}
}

func TestConfig(t *testing.T) {
	up1 := &controller.Upstream{
		Name:      "bookstore-http",
		Namespace: "rc-test",
		Protocol:  "http",
		Port:      80,
		Endpoints: []*controller.Backend{
			&controller.Backend{
				Address:     "127.0.0.1",
				Port:        8080,
				MaxFails:    3,
				FailTimeout: 3,
			},
			&controller.Backend{
				Address:     "127.0.0.1",
				Port:        8081,
				MaxFails:    5,
				FailTimeout: 5,
			},
		},
	}
	up2 := &controller.Upstream{
		Name:      "bookstore-https",
		Namespace: "rc-test",
		Protocol:  "https",
		Port:      80,
		Endpoints: []*controller.Backend{
			&controller.Backend{
				Address:     "127.0.0.1",
				Port:        443,
				MaxFails:    3,
				FailTimeout: 3,
			},
		},
	}
	up3 := &controller.Upstream{
		Name:      "bookstore-grpc",
		Namespace: "default",
		Protocol:  "grpc",
		Port:      80,
		Endpoints: []*controller.Backend{
			&controller.Backend{
				Address:     "127.0.0.1",
				Port:        8001,
				MaxFails:    3,
				FailTimeout: 3,
			},
		},
	}

	pwd, err := os.Getwd()
	check(t, err)

	tmpl := pwd + "/controller/nginx.tmpl"
	nginxConf := pwd + "/nginx.conf"
	tmp := pwd + "/tmp"
	os.Mkdir(tmp, 0700)
	esp := pwd + "/../../src/nginx/main/nginx-esp"
	serviceJson := pwd + "/service.json"
	serviceConfig, err := utils.ServiceConfig("test.appspot.com", "servicecontrol.googleapis.com", nil)
	check(t, err)
	err = ioutil.WriteFile(serviceJson, []byte(serviceConfig), 0644)
	check(t, err)

	conf := controller.NewConfig()
	conf.Upstreams = []*controller.Upstream{up1, up2, up3}
	conf.Servers = []*controller.Server{
		&controller.Server{
			Name: "web.bookstore.org",
			Locations: []*controller.Location{
				&controller.Location{
					Path:              "",
					Upstream:          up1,
					StripPrefix:       false,
					ServiceConfigFile: serviceJson,
					CredentialsFile:   pwd + "/testdata/credentials.json",
				},
				&controller.Location{
					Path:              "/https",
					Upstream:          up2,
					StripPrefix:       false,
					ServiceConfigFile: serviceJson,
				},
			},
			Ports: &controller.Ports{
				SSL:   8443,
				HTTP:  9000,
				HTTP2: 9001,
			},
			SSLCertificate:    pwd + "/testdata/nginx.crt",
			SSLCertificateKey: pwd + "/testdata/nginx.key",
		},
		&controller.Server{
			Name: "",
			Locations: []*controller.Location{
				&controller.Location{
					Path:              "/grpc/",
					Upstream:          up3,
					StripPrefix:       true,
					ServiceConfigFile: serviceJson,
				},
			},
			Ports: &controller.Ports{
				HTTP: 9000,
			},
		},
	}

	conf.UseUpstreamResolver = true
	conf.PID = pwd + "/nginx.pid"
	conf.MimeTypes = ""
	conf.TempDir = tmp

	out, err := os.Create(nginxConf)
	check(t, err)
	err = conf.WriteTemplate(tmpl, out)
	check(t, err)
	out.Close()
	_, err = utils.Run(esp, "-c", nginxConf, "-t")
	check(t, err)

	os.Remove(nginxConf)
	os.Remove(serviceJson)
	os.Remove(conf.PID)
}
