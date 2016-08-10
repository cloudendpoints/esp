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
	"strings"

	"github.com/golang/protobuf/proto"
	"google/api/servicecontrol/v1"
)

func parseOperation(op *servicecontrol.Operation) (configID, requestID, apiKey string) {
	if strings.HasPrefix(op.ConsumerId, "api_key:") {
		apiKey = strings.TrimPrefix(op.ConsumerId, "api_key:")
	} else {
		apiKey = ""
	}

	rf := op.Labels["servicecontrol.googleapis.com/referer"]
	if rf == "" {
		configID = ""
		requestID = ""
	} else {
		a := strings.SplitN(rf, ":", 2)
		if len(a) >= 2 {
			configID = a[0]
			requestID = a[1]
		} else {
			configID = ""
			requestID = a[0]
		}
	}
	return
}

func ParseRequest(url string, body []byte) (configID, requestID, apiKey string) {
	if strings.HasSuffix(url, ":check") {
		cr := servicecontrol.CheckRequest{}
		err := proto.Unmarshal(body, &cr)
		if err == nil {
			return parseOperation(cr.Operation)
		}
	}
	configID = ""
	requestID = ""
	apiKey = ""
	return
}
