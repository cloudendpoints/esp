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
// Bookstore client test
// TODO: convert to go
//
package t

import (
	"deploy"
	"log"
	"utils"
)

func RunBookstore(url string, service deploy.Service) bool {
	path, err := utils.GetTestDataRootPath()
	if err != nil {
		log.Println("Cannot get root path")
		return false
	}
	secret := path + "/client/custom/esp-test-client-secret-jwk.json"
	testToken, err := utils.GenAuthToken(secret, service.Name)

	if err != nil {
		log.Println("Cannot create auth token", err)
		return false
	}

	out, err := utils.Run(path+"/test/client/esp_bookstore_test.py",
		"--verbose=true",
		"--host="+url,
		"--api_key="+service.Key,
		"--auth_token="+testToken,
		"--allow_unverified_cert=true")

	if err != nil {
		log.Println(out)
		return false
	}

	log.Println("Bookstore test passed")
	return true
}
