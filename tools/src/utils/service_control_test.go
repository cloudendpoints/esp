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
	"testing"

	"github.com/golang/protobuf/proto"
	"google/api/servicecontrol/v1"
)

const expectedCheck = `
service_name: "SERVICENAME"
operation: <
  operation_name: "ListShelves"
  consumer_id: "project:endpoints-app"
  labels: <
    key: "servicecontrol.googleapis.com/caller_ip"
    value: "127.0.0.1"
  >
  labels: <
    key: "servicecontrol.googleapis.com/service_agent"
    value: "ESP/0.3.4"
  >
  labels: <
    key: "servicecontrol.googleapis.com/user_agent"
    value: "ESP"
  >
 >
`

func TestCreateCheck(t *testing.T) {
	er := CreateCheck(&ExpectedCheck{
		Version:       "0.3.4",
		ServiceName:   "SERVICENAME",
		ConsumerID:    "project:endpoints-app",
		OperationName: "ListShelves",
		CallerIp:      "127.0.0.1",
	})

	expected := servicecontrol.CheckRequest{}
	if err := proto.UnmarshalText(expectedCheck, &expected); err != nil {
		t.Fatalf("proto.UnmarshalText: %v", err)
	}
	if !proto.Equal(&er, &expected) {
		t.Errorf("Got:\n===\n%v===\nExpected:\n===\n%v===\n", er.String(), expected.String())
	}
}

func TestCreateReport(t *testing.T) {
	er := CreateReport(&ExpectedReport{
		URL:               "/shelves",
		ApiName:           "endpoints-test.cloudendpointsapis.com",
		ApiMethod:         "ListShelves",
		ProducerProjectID: "endpoints-test",
		Location:          "us-central1",
		HttpMethod:        "GET",
		LogMessage:        "Method: ListShelves",
		RequestSize:       39,
		ResponseSize:      208,
		ResponseCode:      503,
		ErrorType:         "5xx",
	})

	testRoot, err := GetTestDataRootPath()
	if err != nil {
		t.Fatalf("failed to get TEST_ROOT")
	}
	text, err := ioutil.ReadFile(testRoot + "/test/src/utils/report_request.golden")
	if err != nil {
		t.Fatalf("failed to read test/src/utils/report_request.golden")
	}

	expected := servicecontrol.ReportRequest{}
	if err := proto.UnmarshalText(string(text), &expected); err != nil {
		t.Fatalf("proto.UnmarshalText: %v", err)
	}
	if !proto.Equal(&er, &expected) {
		var buf bytes.Buffer
		if err := proto.MarshalText(&buf, &er); err != nil {
			t.Errorf("proto.MarsalText: %v", err)
		}
		t.Errorf("Got:\n===\n%v===\nExpected:\n===\n%v===\n", buf.String(), expected.String())
	}
}
