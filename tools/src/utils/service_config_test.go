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
	"fmt"
	"testing"
)

func TestAuthConfig(t *testing.T) {
	providers := []AuthProvider{
		{Issuer: "iss1.com", KeyUrl: "https://xxx", Audiences: "abc, e"},
		{Issuer: "iss2.com", KeyUrl: "https://aaa", Audiences: "xx"},
		{Issuer: "iss3.com"},
		{Issuer: "iss4", Audiences: "yy"},
	}
	config, err := GenerateAuthConfig(providers)
	fmt.Println(config)
	if err != nil {
		t.Errorf("Failed to generate service config: %v", err)
	}
}

func TestServiceConfig(t *testing.T) {
	providers := []AuthProvider{
		{Issuer: "iss1.com", KeyUrl: "https://xxx", Audiences: "abc, e"},
		{Issuer: "iss2.com", KeyUrl: "https://aaa", Audiences: "xx"},
		{Issuer: "iss3.com"},
		{Issuer: "iss4", Audiences: "yy"},
	}
	config, err := ServiceConfig("endpoints-test.cloudendpointsapis.com", "https://test.com:2020", providers)
	fmt.Println(config)
	if err != nil {
		t.Errorf("Failed to generate service config: %v", err)
	}
}
