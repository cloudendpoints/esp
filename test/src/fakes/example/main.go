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
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"net/url"
	"os"
	"strings"

	"github.com/golang/protobuf/proto"
	"google/api/servicecontrol/v1"

	"fakes"
)

func Start() string {
	// To build the fakes binary set the GOPATH to /path/to/esp/test and run:
	// 	go build -o <fakes_binary_path> $GOPATH/src/fakes/server/main.go
	addr, err := fakes.StartFakes("1234", "<fakes_binary_path>")
	if err != nil {
		log.Fatal("Failed to start the fake:", err)
		os.Exit(1)
	}
	return addr
}

func Configure(addr string) string {
	c := fakes.Config{
		ConfigID: "",
		Responses: []fakes.ConfigEntry{
			fakes.ConfigEntry{
				Request: fakes.FakeRequest{
					Url:  "/some/url",
					Verb: "POST",
				},
				Response: fakes.FakeResponse{
					StatusCode: 200,
					Body:       "Body response",
				},
			},
			fakes.ConfigEntry{
				Request: fakes.FakeRequest{
					Url:  "/another/url",
					Verb: "GET",
				},
				Response: fakes.FakeResponse{
					StatusCode: 401,
					Body:       "Not authorized",
				},
			},
			fakes.ConfigEntry{
				Request: fakes.FakeRequest{
					Url:       "/something:check",
					Verb:      "POST",
					RequestID: "Test_Referrer",
				},
				Response: fakes.FakeResponse{
					StatusCode: 401,
					Body:       "Referrer got through",
				},
			},
		},
	}

	fakes.ConfigureFakesUsingConfig(addr, c)
	return addr
}

func Shutdown(addr string) {
	fakes.StopFakes(addr)
}

func TestCheckCall(addr string) {
	// Read the check request from the test file
	pbtxt, err := ioutil.ReadFile("check_request.pb.txt")
	if err != nil {
		log.Fatal(err)
		return
	}

	// Parse the text into a message
	cr := servicecontrol.CheckRequest{}
	err = proto.UnmarshalText(string(pbtxt), &cr)
	if err != nil {
		log.Fatal(err)
		return
	}

	// Serialize the message
	bin, err := proto.Marshal(&cr)

	// Do the call
	c := http.Client{}
	resp, err := c.Post(addr+"/something:check", "application/proto", bytes.NewReader(bin))
	if err != nil {
		log.Fatal(err)
		return
	}

	fmt.Printf("Status - %d\n", resp.StatusCode)

	// Read the Body
	rb, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		log.Fatal(err)
		return
	}

	fmt.Println("Returned Body:")
	fmt.Println(string(rb))
}

func TestCalls(addr string) {
	c := http.Client{}
	r, err := c.Post(addr+"/some/url", "application/proto", strings.NewReader("Test Data bbb"))
	fmt.Println(r.StatusCode, err)

	r, err = c.Post(addr+"/some/url", "application/proto", strings.NewReader("Test Data aaa"))
	fmt.Println(r.StatusCode, err)

	r, err = c.Post(addr+"/some/url", "application/proto", strings.NewReader("Test Data ccc"))
	fmt.Println(r.StatusCode, err)

	r, err = c.Get(addr + "/another/url")
	fmt.Println(r.StatusCode, err)

	r, err = c.Get(addr + "/another/url")
	fmt.Println(r.StatusCode, err)

	req := http.Request{}
	req.Header = map[string][]string{}
	req.Header["MyHeader"] = []string{"This is a header"}
	req.Header["YourHeader"] = []string{"This is your header"}
	req.Method = "GET"
	req.URL, _ = url.Parse(addr + "/another/url")
	r, err = c.Do(&req)
	fmt.Println(r.StatusCode, err)
}

func main() {
	addr := Start()
	Configure(addr)
	TestCheckCall(addr)
	TestCalls(addr)
	rq, err := fakes.GetRequests(addr, "")
	if err != nil {
		log.Fatal(err)
		return
	}
	fmt.Println("Requests:")
	fmt.Println(rq)

	fakes.CleanupFakes(addr, "")

	// these calls should fail with 404 as the fakes server was cleaned up
	TestCheckCall(addr)
	TestCalls(addr)
	rq, err = fakes.GetRequests(addr, "")
	if err != nil {
		log.Fatal(err)
		return
	}

	// should be empty
	fmt.Println("Requests:")
	fmt.Println(rq)

	Shutdown(addr)
}
