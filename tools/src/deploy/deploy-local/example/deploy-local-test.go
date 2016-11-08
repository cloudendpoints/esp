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
	"bytes"
	"deploy/deploy-local"
	"io/ioutil"
	"log"
	"os"
	"strings"
	"time"
)

func getNginxConfFile(parent_dir string) string {
	var r bytes.Buffer

	r.WriteString("pid " + parent_dir + "/" + "nginx.pid;\n")
	r.WriteString("error_log " + parent_dir + "/error.log debug;\n")
	r.WriteString("daemon on;\n")
	r.WriteString("events { worker_connections 32; }\n")
	r.WriteString("\nhttp {\n")
	r.WriteString("  root " + parent_dir + ";\n")
	r.WriteString("  access_log " + parent_dir + "/access.log;\n")
	r.WriteString("  client_body_temp_path " + parent_dir + "/client_body_temp;\n")
	r.WriteString("  proxy_temp_path " + parent_dir + "/proxy_temp;\n")
	r.WriteString("  server_tokens off;\n")
	r.WriteString("  endpoints {\n")
	r.WriteString("    metadata_server http://127.0.0.1:9433;\n")
	r.WriteString("  }\n")
	r.WriteString("  server {\n")
	r.WriteString("    listen 127.0.0.1:9430;\n")
	r.WriteString("    server_name localhost;\n")
	r.WriteString("    location / {\n")
	r.WriteString("      endpoints {\n")
	r.WriteString("        api service.pb.txt;\n")
	r.WriteString("        on;\n")
	r.WriteString("      }\n")
	r.WriteString("      proxy_pass http://127.0.0.1:9431;\n")
	r.WriteString("    }\n")
	r.WriteString("  }\n")
	r.WriteString("}\n")

	return r.String()
}

func getServiceConfFile(src string) (string, error) {
	buf, err := ioutil.ReadFile(src)

	if err != nil {
		return "", err
	}

	return string(buf), err
}

func main() {
	var espExecFile string
	var serviceConfFile string
	var testRunsDir string
	var saveSandbox bool = false

	for _, e := range os.Environ() {
		pair := strings.Split(e, "=")
		if pair[0] == "TEST_RUNS_DIR" {
			testRunsDir = pair[1]
		}

		if pair[0] == "ESP_BINARY" {
			espExecFile = pair[1]
		}

		if pair[0] == "TEST_SVC_CONF" {
			serviceConfFile = pair[1]
		}

		if pair[0] == "TEST_SAVE_SANDBOX" {
			saveSandbox = true
		}
	}

	log.Println("espExecFile = ", espExecFile)
	log.Println("serviceConfFile = ", serviceConfFile)
	log.Println("testRunsDir = ", testRunsDir)

	sandboxDir, err := deploy.CreateSandbox(testRunsDir)

	nginxConf := getNginxConfFile(sandboxDir)
	serviceConf, err := getServiceConfFile(serviceConfFile)

	if err != nil {
		log.Println("Failed to read service conf file: ", serviceConfFile)
	}

	err = deploy.PrepareSandbox(sandboxDir, nginxConf, serviceConf)

	pid, err := deploy.StartEsp(espExecFile, sandboxDir, 5)

	if err == nil {
		log.Println("NGINX running, PID =", pid)
		time.Sleep(3 * time.Second)
		log.Println("Stopping ESP")
		deploy.StopEsp(sandboxDir, !saveSandbox, 5)
	} else {
		log.Fatal("ESP not started, err:", err)
	}
}
