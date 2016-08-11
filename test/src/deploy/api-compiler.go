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
package deploy

import (
	"errors"
	"flag"
	"github.com/golang/protobuf/jsonpb"
	"github.com/golang/protobuf/proto"
	"google/api"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"strings"
)

var (
	ApiCompilerJar string
	JavaBin        string
)

func init() {
	flag.StringVar(&ApiCompilerJar, "api_compiler_jar", "", "Api Compiler binary path")
	flag.StringVar(&JavaBin, "java_bin", GetBinaryPath("java"), "Java binary path")
}

func tempFileName(prefix string) (string, error) {
	f, err := ioutil.TempFile("/tmp", prefix)
	if err != nil {
		return "", err
	}
	if err = f.Close(); err != nil {
		return "", err
	}
	return f.Name(), nil
}

func ApiCompilerCommand(args ...string) ([]byte, error) {
	if ApiCompilerJar == "" {
		return nil, errors.New("api_compiler_jar is not set.")
	}
	if JavaBin == "" {
		return nil, errors.New("java_bin is not set.")
	}
	command := []string{"-jar", ApiCompilerJar}
	command = append(command, args...)
	log.Printf("Running %s %s", JavaBin, strings.Join(command, " "))
	c := exec.Command(JavaBin, command...)
	o, err := c.Output()
	if err == nil {
		log.Printf("API compiler output:\n%s", o)
	}
	return o, err
}

func createServiceConfig(swagger, protobin string) ([]byte, error) {
	return ApiCompilerCommand("--openapi", swagger, "--bin_out", protobin)
}

// Reads binary proto, update Service Control url, save it to json file
func UpdateServiceControlUrl(swagger, url string, json *os.File) error {
	protobin, err := tempFileName("service_proto")
	if err != nil {
		return err
	}
	_, err = createServiceConfig(swagger, protobin)
	if err != nil {
		return err
	}
	serviceproto, err := ioutil.ReadFile(protobin)
	if err != nil {
		return err
	}
	service := &api.Service{}
	err = proto.Unmarshal(serviceproto, service)
	if err != nil {
		return err
	}
	if url != "" {
		service.Control.Environment = url
	}
	m := jsonpb.Marshaler{}
	j, err := m.MarshalToString(service)
	if err != nil {
		return err
	}
	_, err = json.WriteString(j)
	return err
}
