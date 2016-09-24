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
package utils

import (
	"io/ioutil"
	"log"
	"os"
	"strings"
)

func GetTestBinRootPath() (string, error) {
	switch {
	// custom path
	case os.Getenv("TEST_BIN_ROOT") != "":
		return os.Getenv("TEST_BIN_ROOT"), nil
	// running under bazel
	case os.Getenv("TEST_SRCDIR") != "":
		return os.Getenv("TEST_SRCDIR") + "/__main__", nil
	// running with native go
	case os.Getenv("GOPATH") != "":
		list := strings.Split(os.Getenv("GOPATH"), string(os.PathListSeparator))
		return list[0] + "/../bazel-bin", nil
	default:
		return "bazel-bin", nil
	}
}

func GetTestDataRootPath() (string, error) {
	switch {
	// custom path
	case os.Getenv("TEST_DATA_ROOT") != "":
		return os.Getenv("TEST_DATA_ROOT"), nil
	// running under bazel
	case os.Getenv("TEST_SRCDIR") != "":
		return os.Getenv("TEST_SRCDIR") + "/__main__", nil
	// running with native go
	case os.Getenv("GOPATH") != "":
		list := strings.Split(os.Getenv("GOPATH"), string(os.PathListSeparator))
		return list[0] + "/..", nil
	default:
		return "test", nil
	}
}

func GetVersion() (string, error) {
	path, err := GetTestDataRootPath()
	if err != nil {
		return "", err
	}
	ver, err := ioutil.ReadFile(path + "/include/version")
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(ver)), nil
}

func SSLKeyFile() string {
	path, err := GetTestDataRootPath()
	if err != nil {
		log.Fatalln("Cannot find data root path")
	}
	return path + "/test/src/utils/nginx.key"
}

func SSLCertFile() string {
	path, err := GetTestDataRootPath()
	if err != nil {
		log.Fatalln("Cannot find data root path")
	}
	return path + "/test/src/utils/nginx.crt"
}

func CredentialsFile() string {
	path, err := GetTestDataRootPath()
	if err != nil {
		log.Fatalln("Cannot find data root path")
	}
	return path + "/test/src/utils/credentials.json"
}
