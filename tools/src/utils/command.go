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
// A set of shell command utilities
//
package utils

import (
	"crypto/tls"
	"io/ioutil"
	"log"
	"net/http"
	"os/exec"
	"strings"
	"time"
)

// Run command and return the merged output from stderr and stdout, error code
func Run(name string, args ...string) (s string, err error) {
	log.Println(">", name, strings.Join(args, " "))
	c := exec.Command(name, args...)
	bytes, err := c.CombinedOutput()
	s = string(bytes)
	for _, line := range strings.Split(s, "\n") {
		log.Println(line)
	}
	if err != nil {
		log.Println(err)
	}
	return
}

const MaxTries = 10

// Repeat until success (function returns true) up to MaxTries
func Repeat(f func() bool) bool {
	try := 0
	delay := 2 * time.Second
	result := false
	for !result && try < MaxTries {
		if try > 0 {
			log.Println("Waiting for next attempt: ", delay)
			time.Sleep(delay)
			delay = 2 * delay
			log.Println("Repeat attempt #", try+1)
		}
		result = f()
		try = try + 1
	}

	if !result {
		log.Println("Failed all attempts")
	}

	return result
}

// CURL replacement: true if the URL responds with a body
func HTTPGet(url string) bool {
	log.Println("HTTP GET", url)
	timeout := time.Duration(5 * time.Second)
	client := &http.Client{
		Timeout: timeout,
		Transport: &http.Transport{
			TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
		},
	}
	resp, err := client.Get(url)
	if err != nil {
		log.Println(err)
		return false
	}
	defer resp.Body.Close()
	body, _ := ioutil.ReadAll(resp.Body)
	log.Println(string(body))
	return true
}
