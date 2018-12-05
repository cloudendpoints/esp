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
package utils

import (
	"bytes"
	"io/ioutil"
	"text/template"
)

type BasicContext struct {
	ServiceName       string
	ServiceControlUrl string
}

const basicConfig = `
name: "{{.ServiceName}}"
id: "2016-08-25r1"
producer_project_id: "endpoints-test"
http {
  rules {
    selector: "ListShelves"
    get: "/shelves"
  }
  rules {
    selector: "CorsShelves"
    custom: {
      kind: "OPTIONS"
      path: "/shelves"
    }
  }
  rules {
    selector: "CreateShelf"
    post: "/shelves"
  }
  rules {
    selector: "GetShelf"
    get: "/shelves/{shelf}"
  }
  rules {
    selector: "DeleteShelf"
    delete: "/shelves/{shelf}"
  }
  rules {
    selector: "ListBooks"
    get: "/shelves/{shelf}/books"
  }
  rules {
    selector: "CreateBook"
    post: "/shelves/{shelf}/books"
  }
  rules {
    selector: "GetBook"
    get: "/shelves/{shelf}/books/{book}"
  }
  rules {
    selector: "DeleteBook"
    delete: "/shelves/{shelf}/books/{book}"
  }
}
control {
  environment: "{{.ServiceControlUrl}}"
}
usage {
  rules {
    selector: "ListShelves"
    allow_unregistered_calls: false
  }
}
`

type AuthProvider struct {
	Issuer    string
	KeyUrl    string
	Audiences string
}

const authConfig = `
authentication {
  providers: [
   {{range $index, $val := .}}
   {
     id: "issuer{{$index}}"
     issuer: "{{$val.Issuer}}"
     {{if $val.KeyUrl}}jwks_uri: "{{$val.KeyUrl}}"{{end}}
   }{{end}}
  ]
  rules {
    selector: "ListShelves"
    requirements: [
    {{range $index, $val := .}}
      {
        provider_id: "issuer{{$index}}"
        {{if $val.Audiences}}audiences: "{{$val.Audiences}}"{{end}}
      }{{end}}
    ]
  }
}
`

var authTemplates = template.Must(template.New("auth").Parse(authConfig))

func GenerateAuthConfig(providers []AuthProvider) (string, error) {
	var script bytes.Buffer
	if err := authTemplates.Execute(&script, providers); err != nil {
		return "", err
	}
	return script.String(), nil
}

var serviceTemplates = template.Must(template.New("script").Parse(basicConfig))

func readLogMetrics(script *bytes.Buffer) error {
	path, err := GetTestDataRootPath()
	if err != nil {
		return err
	}
	m, err := ioutil.ReadFile(path + "/src/nginx/t/testdata/logs_metrics.pb.txt")
	if err != nil {
		return err
	}
	_, err = script.Write(m)
	return err
}

func GenerateBasicServiceConfig(context *BasicContext) (string, error) {
	var script bytes.Buffer
	if err := serviceTemplates.Execute(&script, context); err != nil {
		return "", err
	}
	script.WriteString("")
	if err := readLogMetrics(&script); err != nil {
		return "", err
	}
	return script.String(), nil
}

func ServiceConfig(serviceName string, serviceControlUrl string, providers []AuthProvider) (string, error) {
	var config bytes.Buffer
	basicPart, err := GenerateBasicServiceConfig(&BasicContext{
		ServiceName:       serviceName,
		ServiceControlUrl: serviceControlUrl,
	})
	if err != nil {
		return "", err
	}
	config.WriteString(basicPart)
	if len(providers) != 0 {
		auth_part, err := GenerateAuthConfig(providers)
		if err != nil {
			return "", err
		}
		config.WriteString(auth_part)
	}
	return config.String(), nil
}
