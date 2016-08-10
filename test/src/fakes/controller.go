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
package fakes

import (
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"os/exec"
	"strings"
	"time"
)

func StartFakes(port, serverPath string) (string, error) {
	cmd := exec.Command(serverPath)
	cmd.Env = append(cmd.Env, fmt.Sprintf("PORT=%s", port))
	err := cmd.Start()
	if err != nil {
		log.Printf("Failed to start the fake. %s", err)
		return "", err
	}
	addr := fmt.Sprintf("http://localhost:%s", port)

	const maxAttempts = 10
	for i := 0; i < maxAttempts; i++ {
		time.Sleep(time.Second)
		client := http.Client{}
		log.Println("Pinging the server...")
		rsp, err := client.Post(addr+"/echo", "text/plain", strings.NewReader("PING"))
		if err == nil && rsp.StatusCode == http.StatusOK {
			log.Println("Got a response...")
			png, err := ioutil.ReadAll(rsp.Body)
			if err == nil && string(png) == "PING" {
				log.Println("Server is up and running...")
				return addr, nil
			}
		}
		log.Println("Could not ping the server. Will wait a second and try again.")
	}

	return "", errors.New("Could not start the fake.")
}

func StopFakes(addr string) error {
	client := http.Client{}
	client.Get(addr + "/shutdown")
	// TODO: Shutdown exits the fake and this call errors out. Figure out how to return OK and then exit.
	//if err != nil {
	//	log.Printf("Failed to stop the fake. %s", err)
	//}
	return nil
}

func ConfigureFakesUsingJson(addr, json string) error {
	client := http.Client{}
	r, err := client.Post(addr+"/configure", "application/json", strings.NewReader(json))
	if err != nil {
		log.Printf("Failed to configure the Fake. %s", err)
		return err
	}
	if r.StatusCode != 200 {
		log.Printf("Failed to configure the Fake. StatusCode=%d, Message=%s", r.StatusCode, r.Status)
		return err
	}
	fmt.Println("Successfully configured:")
	fmt.Println(json)
	return nil
}

func ConfigureFakesUsingConfig(addr string, c Config) error {
	json, err := json.Marshal(c)
	if err != nil {
		log.Printf("Failed to convert config to JSON. %s", err)
		return err
	}
	return ConfigureFakesUsingJson(addr, string(json))
}

func GetRequests(addr, cid string) (Requests, error) {
	client := http.Client{}
	r, err := client.Get(addr + "/requests/" + cid)
	if err != nil {
		log.Printf("Failed to get the requests. %s", err)
		return nil, err
	}
	if r.StatusCode != http.StatusOK {
		log.Printf("Failed to get the requests. %d from fakes server", r.StatusCode)
		return nil, errors.New("Failed to get the requests from fakes server")
	}
	j, err := ioutil.ReadAll(r.Body)
	if err != nil {
		log.Printf("Failed to read the requests. %s", err)
		return nil, err
	}
	rq := Requests{}
	err = json.Unmarshal(j, &rq)
	if err != nil {
		log.Printf("Failed to unmarshal the requests. %s", err)
		return nil, err
	}
	return rq, nil
}

func CleanupFakes(addr, cid string) error {
	client := http.Client{}
	r, err := client.Get(addr + "/cleanup/" + cid)
	if err != nil {
		log.Printf("Failed to cleanup the fakes server. %s", err)
		return err
	}
	if r.StatusCode != http.StatusOK {
		log.Printf("Failed to cleanup the fakes server. The server returned %d", r.StatusCode)
		return err
	}
	return nil
}
