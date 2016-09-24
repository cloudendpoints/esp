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
////////////////////////////////////////////////////////////////////////////////

package controller

import (
	"os"
	"runtime"
	"text/template"
)

type Configuration struct {
	Upstreams      []*Upstream
	Servers        []*Server
	StatusPort     int
	MetadataServer string
	DNSResolver    string
	AccessLog      string
	ErrorLog       string

	UseUpstreamResolver bool

	PID                string
	MimeTypes          string
	NumWorkerProcesses int
	Platform           string
	TempDir            string
}

// Set default values
func (conf *Configuration) Init() {
	conf.PID = "/var/run/nginx.pid"
	conf.MimeTypes = "/etc/nginx/mime.types"
	conf.NumWorkerProcesses = runtime.NumCPU()
	conf.Platform = runtime.GOOS

	conf.MetadataServer = "http://169.254.169.254"
	conf.DNSResolver = "8.8.8.8"
	conf.StatusPort = 8090
	conf.AccessLog = "/dev/stdout"
	conf.ErrorLog = "stderr"
}

type Upstream struct {
	// Service name and port
	Name string
	Port int
	// Valid values are: https, http, grpc
	Protocol string
	// Endpoints
	Endpoints []*Backend
}

type Backend struct {
	Address     string
	Port        int
	MaxFails    int
	FailTimeout int
}

type Ports struct {
	// Positive port implies it is enabled
	SSL, HTTP, HTTP2 int
}

type Server struct {
	Name      string
	Locations []*Location

	Ports             *Ports
	SSLCertificate    string
	SSLCertificateKey string
}

type Location struct {
	Path     string
	Upstream *Upstream

	StripPrefix       bool
	ServiceConfigFile string
	CredentialsFile   string
}

func (conf *Configuration) WriteTemplate(tmplPath string, destPath string) error {
	t, err := template.ParseFiles(tmplPath)
	if err != nil {
		return err
	}

	dest, err := os.Create(destPath)
	if err != nil {
		return err
	}
	defer dest.Close()

	return t.Execute(dest, conf)
}
