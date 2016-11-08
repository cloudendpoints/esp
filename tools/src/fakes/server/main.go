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
	"encoding/json"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"strings"

	"fakes"
)

// Simple echo handler
func echoHandler(w http.ResponseWriter, r *http.Request) {
	b, err := ioutil.ReadAll(r.Body)
	if err != nil {
		log.Fatal(err)
	}
	w.Write(b)
}

// Data structure to store the configuration
type ConfigData struct {
	ConfigID  string
	Responses map[fakes.FakeRequest]fakes.FakeResponse
}

// Configuration map:
// 	<configID> -> ConfigData
var configMap map[string]ConfigData = map[string]ConfigData{}
var requestMap map[string]fakes.Requests = map[string]fakes.Requests{}

// Configures the fake
func configureHandler(w http.ResponseWriter, r *http.Request) {
	// Read the request body
	b, err := ioutil.ReadAll(r.Body)
	if err != nil {
		log.Println(err)
		http.Error(w, err.Error(), 400)
		return
	}

	// Deserialize the requested configuration
	var fcr = fakes.Config{}
	err = json.Unmarshal(b, &fcr)
	if err != nil {
		log.Println(err)
		http.Error(w, err.Error(), 400)
		return
	}

	// Convert FakeConfigRequest to FakeConfig
	fc := ConfigData{ConfigID: fcr.ConfigID, Responses: map[fakes.FakeRequest]fakes.FakeResponse{}}
	for _, resp := range fcr.Responses {
		fc.Responses[resp.Request] = resp.Response
	}

	// Save the configuration
	configMap[fc.ConfigID] = fc

	log.Println("New Configuration:\n")
	log.Println(fc)
}

// Cleans-up a configuration from the config map
func cleanupHandler(w http.ResponseWriter, r *http.Request) {
	configID := ""
	if strings.HasPrefix(r.URL.Path, "/cleanup/") {
		configID = strings.TrimPrefix(r.URL.Path, "/cleanup/")
	}
	delete(configMap, configID)
	delete(requestMap, configID)
}

// Fake handler
func fakeHandler(w http.ResponseWriter, r *http.Request) {
	log.Printf("Fake handler %s", r.URL.Path)

	body := []byte{}
	var configID, requestID, apiKey string = "", "", ""
	if r.Method == "POST" {
		var err error
		body, err = ioutil.ReadAll(r.Body)
		if err == nil && len(body) > 0 {
			configID, requestID, apiKey = fakes.ParseRequest(r.URL.Path, body)
		} else {
			body = []byte{}
		}
	}

	fakeConf, present := configMap[configID]
	if !present {
		log.Printf("Nothing configuted for config ID %s", configID)
		http.Error(w, "Not Found", 404)
		return
	}

	log.Printf("ConfigID: %s, RequestID: %s, ApiKey: %s", configID, requestID, apiKey)

	fr := fakes.FakeRequest{
		Url:       r.URL.Path,
		Verb:      r.Method,
		RequestID: requestID,
		ApiKey:    apiKey,
	}

	requestMap[configID] = append(requestMap[configID],
		fakes.RequestData{
			Url:       r.URL.Path,
			Verb:      r.Method,
			RequestID: requestID,
			Body:      body,
			Headers:   fakes.FlattenHeaders(r.Header),
		})

	// Try to find a fake request in the config
	response, present := fakeConf.Responses[fr]
	if present {
		w.WriteHeader(response.StatusCode)
		w.Write([]byte([]byte(response.Body)))
		return
	}

	// Now, clear the request ID and try to find again
	if fr.RequestID != "" {
		fr.RequestID = ""
		response, present = fakeConf.Responses[fr]
		if present {
			w.WriteHeader(response.StatusCode)
			w.Write([]byte([]byte(response.Body)))
			return
		}
	}

	// Try without the api key
	if fr.ApiKey != "" {
		fr.ApiKey = ""
		fr.RequestID = requestID // restore request ID
		response, present = fakeConf.Responses[fr]
		if present {
			w.WriteHeader(response.StatusCode)
			w.Write([]byte([]byte(response.Body)))
			return
		}

		// Without api key & without request ID
		if fr.RequestID != "" {
			fr.RequestID = ""
			response, present = fakeConf.Responses[fr]
			if present {
				w.WriteHeader(response.StatusCode)
				w.Write([]byte([]byte(response.Body)))
				return
			}
		}
	}

	// No luck :(
	log.Printf("Nothing configured for path %s", r.URL.Path, r.Method)
	http.Error(w, "Not Found", 404)
}

// Get the requests for a config id
func requestsHandler(w http.ResponseWriter, r *http.Request) {
	log.Printf("Requests handler %s", r.URL.Path)

	configID := ""
	if strings.HasPrefix(r.URL.Path, "/requests/") {
		configID = strings.TrimPrefix(configID, "/requests/")
	}

	json, err := json.Marshal(requestMap[configID])
	if err != nil {
		http.Error(w, "Failed to marshal the requests", 500)
		return
	}
	w.WriteHeader(http.StatusOK)
	w.Write(json)
}

// Shuts down the fake server
func shutdownHandler(w http.ResponseWriter, r *http.Request) {
	log.Println("Shutting down...")
	http.Error(w, "Shutting down...", 200)
	os.Exit(0)
}

func main() {
	var address string
	if os.Getenv("PORT") != "" {
		address = ":" + os.Getenv("PORT")
	} else {
		address = ":8081" // The default port is 8081
	}

	http.HandleFunc("/", fakeHandler)
	http.HandleFunc("/echo", echoHandler)
	http.HandleFunc("/configure", configureHandler)
	http.HandleFunc("/cleanup/", cleanupHandler)
	http.HandleFunc("/requests", requestsHandler)
	http.HandleFunc("/requests/", requestsHandler)
	http.HandleFunc("/shutdown", shutdownHandler)

	log.Printf("Start serving on port %s\n", address)
	log.Fatal(http.ListenAndServe(address, nil))
}
