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
package t

import (
	"fmt"
	"io/ioutil"
	"net/http"
	"net/url"
	"os"
	"strings"
	"testing"
	"utils"
)

var ts TestSetup

const (
	backendPort        = 8090
	nginxPort          = 8080
	serviceControlPort = 8091
	metadataPort       = 8092
	pubKeyPort         = 8093
	serviceName        = "SERVICENAME"
	sandboxPath        = "/tmp"
	serviceConfigPath  = "service.pb.txt"
)

// bazel rules don't understand TestMain so we need to work around it
func TestStart(t *testing.T) {

	var err error

	ts, err = SetupLocal(
		backendPort,
		nginxPort,
		serviceControlPort,
		metadataPort,
		pubKeyPort,
		serviceName,
		sandboxPath,
		serviceConfigPath,
		true, // authentication on
	)

	if err != nil {
		t.Fatal("Error setting up")
	}

}
func TestPKS(t *testing.T) {

	const response = `
	{
	 "keys": [
	  {
	   "kty": "RSA",
	   "alg": "RS256",
	   "use": "sig",
	   "kid": "62a93512c9ee4c7f8067b5a216dade2763d32a47",
	   "n": "0YWnm_eplO9BFtXszMRQNL5UtZ8HJdTH2jK7vjs4XdLkPW7YBkkm_2xNgcaVpkW0VT2l4mU3KftR-6s3Oa5Rnz5BrWEUkCTVVolR7VYksfqIB2I_x5yZHdOiomMTcm3DheUUCgbJRv5OKRnNqszA4xHn3tA3Ry8VO3X7BgKZYAUh9fyZTFLlkeAh0-bLK5zvqCmKW5QgDIXSxUTJxPjZCgfx1vmAfGqaJb-nvmrORXQ6L284c73DUL7mnt6wj3H6tVqPKA27j56N0TB1Hfx4ja6Slr8S4EB3F1luYhATa1PKUSH8mYDW11HolzZmTQpRoLV8ZoHbHEaTfqX_aYahIw",
	   "e": "AQAB"
	  },
	  {
	   "kty": "RSA",
	   "alg": "RS256",
	   "use": "sig",
	   "kid": "b3319a147514df7ee5e4bcdee51350cc890cc89e",
	   "n": "qDi7Tx4DhNvPQsl1ofxxc2ePQFcs-L0mXYo6TGS64CY_2WmOtvYlcLNZjhuddZVV2X88m0MfwaSA16wE-RiKM9hqo5EY8BPXj57CMiYAyiHuQPp1yayjMgoE1P2jvp4eqF-BTillGJt5W5RuXti9uqfMtCQdagB8EC3MNRuU_KdeLgBy3lS3oo4LOYd-74kRBVZbk2wnmmb7IhP9OoLc1-7-9qU1uhpDxmE6JwBau0mDSwMnYDS4G_ML17dC-ZDtLd1i24STUw39KH0pcSdfFbL2NtEZdNeam1DDdk0iUtJSPZliUHJBI_pj8M-2Mn_oA8jBuI8YKwBqYkZCN1I95Q",
	   "e": "AQAB"
	  }
	 ]
	}
	`

	url := "/pubkey"

	ts.publicKeyServer.Config(url, http.MethodGet, http.StatusOK, response)

	client := http.Client{}
	res, error := client.Get(ts.publicKeyServer.address + url)
	if error != nil {
		t.Errorf("request failed.")
	}
	if res.StatusCode != http.StatusOK {
		t.Errorf("Expected status 200, got:", res.StatusCode)
	}

	body, _ := ioutil.ReadAll(res.Body)
	if string(body) != response {
		t.Errorf("Wrong response body.")
	}
}

func TestE2e(t *testing.T) {

	// PKS must be setup for this test to run

	var testDataRoot, err = utils.GetTestDataRootPath()
	if err != nil {
		t.Errorf("Test env path is not setup correctly.")
	}

	var secretFile = testDataRoot + "/src/nginx/t/matching-client-secret.json"
	token, err := utils.GenAuthToken(secretFile, "ok_audience_1")
	t.Log(token)

	queryUrl := "/shelves"

	// get back data and verify the results
	var expectedResponse = `{ "shelves": [
		{ "name": "shelves/1", "theme": "Fiction" },
		{ "name": "shelves/2", "theme": "Fantasy" }
	]
	}`

	ts.backend.Config(queryUrl, http.MethodGet, http.StatusOK, expectedResponse)
	ts.serviceController.Config(http.StatusOK, "", http.StatusOK, "")

	// issue request and verify response
	client := http.Client{}
	req := http.Request{}
	req.Header = map[string][]string{}
	req.Header["Authorization"] = []string{"Bearer " + token}
	req.Method = http.MethodGet
	req.URL, _ = url.Parse(
		fmt.Sprintf("http://127.0.0.1:%d%s?api_key=api-key-1", nginxPort, queryUrl))
	res, err := client.Do(&req)

	check_auth := req.Header.Get("Authorization")
	t.Log("Check header of auth: ")
	t.Log(check_auth)

	if err != nil {
		t.Errorf("Request failed.")
	}
	if res.StatusCode != http.StatusOK {
		t.Errorf("Expected code: %d, got:", http.StatusOK, res.StatusCode)
	}

	body, _ := ioutil.ReadAll(res.Body)
	if string(body) != expectedResponse {
		t.Errorf("Wrong response body.")
	}
}

func TestMetadataRequests(t *testing.T) {

	// WARNING! This test assumes that all other tests have run
	// it cannot be run in isolation
	rq, err := ts.metadataServer.GetRequests()
	if err != nil {
		t.Errorf("Failed to get request data from metadata server")
	}

	t.Log(rq)

	// ESP only issues two requests, but createMetadataServer() issues one
	// for testing.
	if len(rq) != 3 {
		t.Errorf("Number of requests doesn't match")
	}

	// first one
	meta := rq[1]
	if meta.Url != ts.metadataServer.metaUrl {
		t.Errorf("meta URL doesn't match")
	}
	if meta.Verb != http.MethodGet {
		t.Errorf("meta Verb doesn't match")
	}

	// second one
	token := rq[2]
	if token.Url != ts.metadataServer.tokenUrl {
		t.Errorf("token URL doesn't match")
	}
	if token.Verb != http.MethodGet {
		t.Errorf("token Verb doesn't match")
	}
}

func TestServiceControlRequests(t *testing.T) {

	// WARNING! This test assumes that all other tests have run
	// it cannot be run in isolation
	rq, err := GetServiceControlData(fmt.Sprintf("http://127.0.0.1:%d", serviceControlPort), 3, 5)
	if err != nil {
		t.Errorf("Failed to get request data from service control server")
	}

	// first one is Check
	r := rq[1]
	if !strings.HasSuffix(r.Url, ":check") {
		t.Errorf("wrong check URL: %s", r.Url)
	}
	if r.Verb != "POST" {
		t.Errorf("wrong check verb: %s", r.Verb)
	}

	ver, err := utils.GetVersion()
	if err != nil {
		t.Errorf("Failed to get expected version.")
	}

	if !utils.VerifyCheck(r.Body, &utils.ExpectedCheck{
		Version:       ver,
		ServiceName:   "SERVICENAME",
		ConsumerID:    "api_key:api-key-1",
		OperationName: "ListShelves",
		CallerIp:      "127.0.0.1",
	}) {
		t.Errorf("Check request data doesn't match.")
	}

	// second one is Report
	r = rq[2]
	if !strings.HasSuffix(r.Url, ":report") {
		t.Errorf("wrong report URL: %s", r.Url)
	}
	if r.Verb != "POST" {
		t.Errorf("wrong report verb: %s", r.Verb)
	}
	if !utils.VerifyReport(r.Body, &utils.ExpectedReport{
		Version:           ver,
		ApiKey:            "api-key-1",
		URL:               "/shelves?api_key=api-key-1",
		ApiName:           "SERVICENAME",
		ApiMethod:         "ListShelves",
		Platform:          "GCE",
		ProducerProjectID: "endpoints-app",
		Location:          "us-west1-d",
		HttpMethod:        "GET",
		LogMessage:        "Method: ListShelves",
		RequestSize:       894,
		ResponseSize:      270,
		ResponseCode:      200,
	}) {
		t.Errorf("Report request data doesn't match.")
	}
}

func TestCleanup(t *testing.T) {
	saveSandbox := os.Getenv("T_TEST_SAVE_SANDBOX") != ""

	TearDownLocal(ts, saveSandbox)
}
