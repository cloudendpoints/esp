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
	"deploy"
	"io/ioutil"

	"github.com/golang/glog"
)

// ConfigStore manages a local cache of service configs
type ConfigStore interface {
	// Get returns a file name for the service config
	GetConfig(name, version string) (string, error)
}

type ConfigId struct {
	name, id string
}

type ConfigStoreImpl struct {
	files  map[ConfigId]string
	root   string
	client *deploy.Service
}

func MakeConfigStore(root, credentials string) (ConfigStore, error) {
	client := &deploy.Service{CredentialsFile: credentials}
	err := client.Connect()
	if err != nil {
		return nil, err
	}
	return &ConfigStoreImpl{
		root:   root,
		client: client,
		files:  map[ConfigId]string{},
	}, nil
}

// TODO: empty version should not be allowed here to preserve the cache semantics
func (store *ConfigStoreImpl) GetConfig(name, version string) (string, error) {
	id := ConfigId{name: name, id: version}
	file, ok := store.files[id]
	if ok {
		return file, nil
	}

	glog.Infof("Fetching service config %s:%s", name, version)

	store.client.Name = name
	store.client.Version = version
	out, err := store.client.Fetch()
	if err != nil {
		return "", err
	}

	bytes, err := out.MarshalJSON()
	if err != nil {
		return "", err
	}

	file = store.root + "/" + name + "-" + version + ".json"
	err = ioutil.WriteFile(file, bytes, 0644)
	if err != nil {
		return "", err
	}

	store.files[id] = file
	return file, nil
}
