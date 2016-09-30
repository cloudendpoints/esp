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
	"fmt"
	"io"
	"runtime"
	"text/template"

	"github.com/golang/glog"
)

type Configuration struct {
	Upstreams []*Upstream
	Servers   []*Server

	StatusPort          int
	MetadataServer      string
	DNSResolver         string
	AccessLog           string
	ErrorLog            string
	UseUpstreamResolver bool

	PID                string
	MimeTypes          string
	NumWorkerProcesses int
	Platform           string
	TempDir            string
}

// Set default values
func NewConfig() *Configuration {
	return &Configuration{
		StatusPort:          8090,
		MetadataServer:      "http://169.254.169.254",
		DNSResolver:         "8.8.8.8",
		AccessLog:           "/dev/stdout",
		ErrorLog:            "stderr",
		UseUpstreamResolver: false,

		PID:                "/var/run/nginx.pid",
		MimeTypes:          "/etc/nginx/mime.types",
		NumWorkerProcesses: runtime.NumCPU(),
		Platform:           runtime.GOOS,
		// use default values for temp dir
		TempDir: "",
	}
}

type Upstream struct {
	// Service name, namespace, port, and protocl
	Name      string
	Namespace string
	Port      int
	Protocol  string

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

func IsValidProtocol(proto string) bool {
	return proto == "http" || proto == "https" || proto == "grpc"
}

// Label uniquely identifies upstream
func (up *Upstream) Label() string {
	return fmt.Sprintf("%s_%s_%s_%d", up.Protocol, up.Name, up.Namespace, up.Port)
}

func (up *Upstream) Address() string {
	return fmt.Sprintf("%s.%s", up.Name, up.Namespace)
}

func (conf *Configuration) WriteTemplate(tmplPath string, out io.Writer) error {
	t, err := template.ParseFiles(tmplPath)
	if err != nil {
		return err
	}

	return t.Execute(out, conf)
}

func (conf *Configuration) DeduplicateUpstreams() {
	upstreams := make([]*Upstream, 0)
	set := map[string]bool{}
	for _, upstream := range conf.Upstreams {
		if _, exists := set[upstream.Label()]; exists {
			continue
		}
		upstreams = append(upstreams, upstream)
		set[upstream.Label()] = true
	}
	conf.Upstreams = upstreams
}

func (conf *Configuration) RemoveEmptyUpstreams() {
	upstreams := make([]*Upstream, 0)
	for _, upstream := range conf.Upstreams {
		if len(upstream.Endpoints) > 0 {
			upstreams = append(upstreams, upstream)
		} else {
			glog.Warningf("Upstream %v does not have any active endpoints", upstream.Label())
		}
	}
	conf.Upstreams = upstreams
}

func (conf *Configuration) RemoveStaleLocations() {
	set := map[string]bool{}
	for _, upstream := range conf.Upstreams {
		set[upstream.Label()] = true
	}

	servers := make([]*Server, 0)
	for _, server := range conf.Servers {
		locations := make([]*Location, 0)
		for _, location := range server.Locations {
			if _, exists := set[location.Upstream.Label()]; exists {
				locations = append(locations, location)
			} else {
				glog.Warningf("Missing upstream %s for location %s",
					location.Upstream.Label(), location.Path)
			}
		}

		if len(locations) > 0 {
			server.Locations = locations
			servers = append(servers, server)
		}
	}
	conf.Servers = servers
}
