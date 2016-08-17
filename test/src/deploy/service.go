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
package deploy

import (
	"bytes"
	"errors"
	"io/ioutil"
	"log"
	"os"
	"path"
	"text/template"
)

var (
	ServiceManagement = []string{"beta", "service-management"}
)

type Service struct {
	Name    string
	Version string
	Token   string
	Key     string

	// Creation parameters
	Delete              bool
	ConsumerProjectId   string
	ProducerProjectId   string
	ServiceControlUrl   string
	ServiceJsonPath     string
	SwaggertemplatePath string
}

// Creates the swagger json from template
// Convert to proto with Api Manager
// Change chemist to point to what we want
// Convert back to service.json
// Set ServiceJsonPath
func (s *Service) Configure() error {
	// Template to swagger json
	log.Printf("Configuring Service Config")
	d, err := ioutil.TempDir("/tmp", "svcman_files")
	if err != nil {
		return err
	}
	sfilename := path.Join(d, "swagger.json")
	swagger, err := os.Create(sfilename)
	if err != nil {
		return err
	}
	if err = s.createSwaggerJson(swagger); err != nil {
		return err
	}
	if err = swagger.Close(); err != nil {
		return err
	}
	jsonfile, err := ioutil.TempFile("/tmp", "service_json")
	if err != nil {
		return err
	}
	err = UpdateServiceControlUrl(sfilename, s.ServiceControlUrl, jsonfile)
	if err != nil {
		return err
	}
	s.ServiceJsonPath = jsonfile.Name()
	return jsonfile.Close()
}

func (s *Service) SetUp() error {
	log.Printf("Setting up service %s", s.Name)
	if !s.setVersion() {
		// Set Version
		if _, err := s.createService(); err != nil {
			return err
		}
		if !s.setVersion() {
			return errors.New("Could not set version.")
		}
		_, err := s.enableService()
		return err
	}
	return nil
}

func (s *Service) TearDown() error {
	if !s.Delete {
		return nil
	}
	log.Printf("Tearing down service %s", s.Name)
	// Disabling service first
	if err := s.disableService(); err != nil {
		return err
	}
	if err := s.deleteService(); err != nil {
		return err
	}
	log.Printf("Unsetting s.Version")
	s.Version = ""
	return nil
}

func (s *Service) serviceManagementCommand(args ...string) (interface{}, error) {
	command := append(ServiceManagement, args...)
	return GcloudCommand(command...)
}

// Create Service
func (s *Service) createService() (interface{}, error) {
	if s.ServiceJsonPath == "" {
		return nil, errors.New("No swagger file is set.")
	}
	return s.serviceManagementCommand("deploy", s.ServiceJsonPath, "--project", s.ProducerProjectId)
}

// Create a Swagger Json from template and save it to json file
func (s *Service) createSwaggerJson(f *os.File) error {
	t, err := ioutil.ReadFile(s.SwaggertemplatePath)
	if err != nil {
		log.Printf("Unable to read %s", s.SwaggertemplatePath)
		return err
	}
	log.Printf("Creating json file from template %s", s.SwaggertemplatePath)
	tmpl, err := template.New(path.Base(s.SwaggertemplatePath)).Parse(string(t))
	if err != nil {
		return err
	}
	var doc bytes.Buffer
	if err = tmpl.Execute(&doc, s); err != nil {
		return err
	}
	_, err = f.WriteString(doc.String())
	return err
}

// Delete Service
func (s *Service) deleteService() error {
	_, err := s.serviceManagementCommand("delete", "-q", s.Name, "--project", s.ProducerProjectId)
	return err
}

// Describe service
func (s *Service) describeService() (interface{}, error) {
	return s.serviceManagementCommand("describe", s.Name, "--project", s.ProducerProjectId)
}

// Disable Service
func (s *Service) disableService() error {
	_, err := s.serviceManagementCommand("disable", s.Name, "--project", s.ConsumerProjectId)
	return err
}

// Enable Service
func (s *Service) enableService() (interface{}, error) {
	return s.serviceManagementCommand("enable", s.Name, "--project", s.ConsumerProjectId)
}

// Check service existence and set s.version
func (s *Service) setVersion() bool {
	if s.Version != "" {
		return true
	}
	version, err := s.GetVersion()
	if err == nil {
		s.Version = version
		return true
	} else {
		return false
	}
}

func (s *Service) GetVersion() (string, error) {
	f, err := s.describeService()
	if err == nil {
		m := f.(map[string]interface{})
		sc, ok := m["serviceConfig"].(map[string]interface{})
		if !ok {
			return "", errors.New("Service config format")
		}
		id, ok := sc["id"].(string)
		if !ok {
			return "", errors.New("Service config format")
		}
		log.Printf("Version of %s is %s", s.Name, id)
		return id, nil
	}

	return "", err
}
