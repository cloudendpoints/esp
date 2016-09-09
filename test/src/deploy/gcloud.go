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
	"encoding/json"
	"errors"
	"flag"
	"log"
	"os/exec"
	"strings"
)

var (
	GcloudBinary string
)

func init() {
	flag.StringVar(&GcloudBinary, "gcloud_bin", GetBinaryPath("gcloud"), "Gcloud binary path")
}

func GetBinaryPath(binary string) string {
	c := exec.Command("which", binary)
	o, err := c.Output()
	if err != nil {
		return ""
	}
	return strings.TrimSpace(string(o[:]))
}

func GcloudCommand(args ...string) (interface{}, error) {
	if GcloudBinary == "" {
		return nil, errors.New("gcloud_bin is not set.")
	}
	log.Printf("Running %s %s", GcloudBinary, strings.Join(args, " "))
	command := append(args, "--format=json")
	c := exec.Command(GcloudBinary, command...)
	o, err := c.Output()
	if err != nil {
		log.Println("Error: ", err)
		return nil, err
	}
	var f interface{}
	if err := json.Unmarshal(o, &f); err != nil {
		return nil, err
	}
	return f, nil
}
