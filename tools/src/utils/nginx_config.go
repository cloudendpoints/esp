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
	"text/template"
)

type scriptContext struct {
	// Nginx port
	NginxPort int
	// Backend port
	BackendPort int
	// Service config path
	ServiceConfigPath string
	// Server config path
	ServerConfigPath string
	// Test Sandbox path
	SandboxPath string
	// Metadata server
	MetadataServer string
}

var templates = template.Must(template.New("scripts").Parse(config))

const config = `
pid {{.SandboxPath}}/nginx.pid;
error_log {{.SandboxPath}}/error.log debug;
daemon off;
events {
  worker_connections 32;
}
http {
  root {{.SandboxPath}};
  access_log {{.SandboxPath}}/access.log;
  client_body_temp_path {{.SandboxPath}}/client_body_temp;
  proxy_temp_path {{.SandboxPath}}/proxy_temp;
  server_tokens off;
  {{if .MetadataServer}}
  endpoints {
    metadata_server {{.MetadataServer}};
  }
  {{end}}
  server {
    listen 127.0.0.1:{{.NginxPort}};
    server_name localhost;
    location / {
      endpoints {
        api {{.ServiceConfigPath}};
        {{if .ServerConfigPath}}
        server_config {{.ServerConfigPath}};
        {{end}}
        on;
      }
      proxy_pass http://127.0.0.1:{{.BackendPort}};
    }
  }
}
`

func execute(context *scriptContext) (string, error) {
	var script bytes.Buffer
	if err := templates.Execute(&script, context); err != nil {
		return "", err
	}
	return script.String(), nil
}

func NginxConfig(nginxPort int, backendPort int, serviceConfigPath string,
	sandboxPath string, metadataServer string) (string, error) {
	return execute(&scriptContext{
		NginxPort:         nginxPort,
		BackendPort:       backendPort,
		ServiceConfigPath: serviceConfigPath,
		ServerConfigPath:  "",
		SandboxPath:       sandboxPath,
		MetadataServer:    metadataServer,
	})
}
