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
////////////////////////////////////////////////////////////////////////////////

package controller

import (
	"fmt"
	"strings"

	"github.com/golang/glog"

	extensions "k8s.io/client-go/1.5/pkg/apis/extensions/v1beta1"
)

func IsESPIngress(i *extensions.Ingress) bool {
	val, ok := i.Annotations["kubernetes.io/ingress.class"]
	return ok && val == "esp"
}

func GetBackendProtocol(i *extensions.Ingress) string {
	proto, ok := i.Annotations["googleapis.com/backend-protocol"]
	if ok && IsValidProtocol(proto) {
		return proto
	}

	if ok {
		glog.Warningf("Unsupported service backend protocol %v, using http", proto)
	}

	return "http"
}

func GetServiceConfig(i *extensions.Ingress) (string, string, error) {
	name, ok := i.Annotations["googleapis.com/service-name"]
	if !ok {
		return "", "", fmt.Errorf("Missing service name for ingress %s", i.Name)
	}
	version, ok := i.Annotations["googleapis.com/service-config-id"]
	if !ok {
		glog.Warningf("Missing service config ID for ingress %s", i.Name)
		version = ""
	}

	return name, version, nil
}

func GetStripPrefix(i *extensions.Ingress) bool {
	val, ok := i.Annotations["googleapis.com/strip-prefix"]
	return ok && strings.ToLower(val) == "true"
}
