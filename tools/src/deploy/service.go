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
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"strings"
	"time"

	mgmt "google.golang.org/api/servicemanagement/v1"

	"golang.org/x/net/context"
	"golang.org/x/oauth2/google"
	"golang.org/x/oauth2/jwt"

	"gopkg.in/yaml.v2"
)

// Service handler for the API
type Service struct {
	Name            string
	Version         string
	CredentialsFile string
	ProducerProject string

	hc  *http.Client
	api *mgmt.APIService
}

// GetClient with given scope
func (s *Service) GetClient(scope string) (hc *http.Client, err error) {
	ctx := context.Background()
	if s.CredentialsFile != "" {
		log.Println("Using credentials file:", s.CredentialsFile)
		var jsonKey []byte
		jsonKey, err = ioutil.ReadFile(s.CredentialsFile)
		if err != nil {
			return nil, err
		}

		var config *jwt.Config
		config, err = google.JWTConfigFromJSON(jsonKey, scope)
		if err != nil {
			return nil, err
		}

		// fix producer project if it is missing
		if s.ProducerProject == "" {
			var key struct {
				ProjectID string `json:"project_id"`
			}
			if err = json.Unmarshal(jsonKey, &key); err == nil {
				s.ProducerProject = key.ProjectID
				log.Println("Extracted producer project ID:", s.ProducerProject)
			} else {
				log.Println("Failed to extract project_id from the credentials file")
			}
		}

		hc = config.Client(ctx)
	} else {
		log.Println("Using default oauth2 client")
		hc, err = google.DefaultClient(ctx, scope)
		if err != nil {
			return
		}
	}
	return
}

// Connect to service management service
func (s *Service) Connect() (err error) {
	s.hc, err = s.GetClient(mgmt.ServiceManagementScope)
	if err != nil {
		return
	}
	s.api, err = mgmt.New(s.hc)
	if err != nil {
		return
	}

	return nil
}

// Await polls for completion of the operation
func (s *Service) Await(op *mgmt.Operation) *mgmt.Operation {
	try := 0
	delay := time.Second
	MaxTries := 10

	var err error
	for !op.Done && try < MaxTries {
		if try > 0 {
			if try > 3 {
				log.Println("...Sleeping", delay, "before operation status check #", try+1)
			}
			time.Sleep(delay)
			delay = 2 * delay
		}
		op, err = s.api.Operations.Get(op.Name).Do()
		if err != nil {
			log.Println("Failed to retrieve the operation status", err)
			return op
		}
		try = try + 1
	}

	return op
}

// AwaitDone polls and checks for the completion
func (s *Service) AwaitDone(op *mgmt.Operation) error {
	if s.Await(op).Done {
		return nil
	}
	return errors.New("Failed to complete operation: " + op.Name)
}

// Fetch service configuration from the handler
func (s *Service) Fetch() (svc *mgmt.Service, err error) {
	if s.Name == "" {
		err = errors.New("Missing service name")
	} else if s.Version == "" {
		svc, err = s.api.Services.GetConfig(s.Name).Do()
		if err == nil {
			log.Println("Using the latest service config ID:", svc.Id)
			s.Version = svc.Id
		}
	} else {
		svc, err = s.api.Services.Configs.Get(s.Name, s.Version).Do()
	}
	return
}

// Exists checks if service name is registered
func (s *Service) Exists() bool {
	_, err := s.api.Services.Get(s.Name).Do()
	return err == nil
}

// Create a service with a given name
func (s *Service) Create() error {
	log.Println("Create service", s.Name)
	if s.ProducerProject == "" {
		log.Println("Please specify the producer project")
	}
	op, err := s.api.Services.Create(&mgmt.ManagedService{
		ServiceName:       s.Name,
		ProducerProjectId: s.ProducerProject,
	}).Do()
	if err != nil {
		return err
	}
	return s.AwaitDone(op)
}

// Enable a managed service for a consumer project
func (s *Service) Enable(name, project string) error {
	log.Printf("Enable service %s for consumer project %s", name, project)
	op, err := s.api.Services.Enable(name, &mgmt.EnableServiceRequest{
		ConsumerId: "project:" + project,
	}).Do()
	if err != nil {
		return err
	}
	return s.AwaitDone(op)
}

// Ensure that the service exists and has dependencies enabled
func (s *Service) Ensure() error {
	if s.ProducerProject == "" {
		return errors.New("Please specify the producer project")
	}

	if err := s.Enable("endpoints.googleapis.com", s.ProducerProject); err != nil {
		return err
	}

	if !s.Exists() {
		log.Printf("Service %s does not exist\n", s.Name)
		err := s.Create()
		if err != nil {
			return err
		}
	}

	return nil
}

// Delete service by name
func (s *Service) Delete() error {
	log.Println("Delete service", s.Name)
	op, err := s.api.Services.Delete(s.Name).Do()
	if err != nil {
		return err
	}
	return s.AwaitDone(op)
}

// Undelete service by name
func (s *Service) Undelete() error {
	log.Println("Undelete service", s.Name)
	op, err := s.api.Services.Undelete(s.Name).Do()
	if err != nil {
		return err
	}
	return s.AwaitDone(op)
}

// ConfigSource provides minimal schema for YAML config
type ConfigSource struct {
	Swagger *string `yaml:"swagger"`
	Host    *string `yaml:"host"`
	Name    *string `yaml:"name"`
	Type    *string `yaml:"type"`
}

// CreateConfigFiles loads files and returns config sources
// and updates the service name if it can be extracted
func (s *Service) CreateConfigFiles(files []string) ([]*mgmt.ConfigFile, error) {
	var configFiles []*mgmt.ConfigFile
	for _, file := range files {
		contents, err := ioutil.ReadFile(file)
		if err != nil {
			return nil, errors.New("Cannot read file " + file)
		}

		var fileType string

		// YAML/JSON vs proto descriptor
		if strings.HasSuffix(strings.ToUpper(file), ".YAML") ||
			strings.HasSuffix(strings.ToUpper(file), ".YML") ||
			strings.HasSuffix(strings.ToUpper(file), ".JSON") {
			t := ConfigSource{}
			err := yaml.Unmarshal([]byte(contents), &t)
			if err != nil {
				return nil, errors.New("Cannot parse as YAML: " + file)
			}
			if t.Swagger != nil {
				// Always use YAML for input
				fileType = "OPEN_API_YAML"
				if s.Name == "" && t.Host != nil {
					s.Name = *t.Host
				}
			} else if t.Type != nil && *t.Type == "google.api.Service" {
				fileType = "SERVICE_CONFIG_YAML"
				if s.Name == "" && t.Name != nil {
					s.Name = *t.Name
				}
			} else {
				return nil, errors.New("Unsupported config source" + file)
			}
		} else if strings.HasSuffix(strings.ToUpper(file), ".PB") ||
			strings.HasSuffix(strings.ToUpper(file), ".DESCRIPTOR") {
			fileType = "FILE_DESCRIPTOR_SET_PROTO"
		}

		configFiles = append(configFiles, &mgmt.ConfigFile{
			FileContents: base64.StdEncoding.EncodeToString(contents),
			FileType:     fileType,
			FilePath:     file,
		})
	}

	return configFiles, nil
}

// Submit creates a service config from config sources
func (s *Service) Submit(configFiles []*mgmt.ConfigFile) (*mgmt.Service, error) {
	log.Println("Submit service configuration for", s.Name)

	req := &mgmt.SubmitConfigSourceRequest{
		ConfigSource: &mgmt.ConfigSource{
			Files: configFiles,
		},
	}

	op, err := s.api.Services.Configs.Submit(s.Name, req).Do()
	if err != nil {
		return nil, err
	}

	op = s.Await(op)
	if !op.Done {
		return nil, errors.New("Failed to complete operation " + op.Name)
	}

	res := &mgmt.SubmitConfigSourceResponse{}
	err = remarshal(op.Response, res)
	if err != nil {
		return nil, err
	}

	log.Println("Successfully submitted config version", res.ServiceConfig.Id)
	s.Version = res.ServiceConfig.Id
	return res.ServiceConfig, nil
}

// Rollout pushes the config the backend services
func (s *Service) Rollout() error {
	log.Println("Rollout service " + s.Name + " config " + s.Version)

	op, err := s.api.Services.Rollouts.Create(s.Name, &mgmt.Rollout{
		TrafficPercentStrategy: &mgmt.TrafficPercentStrategy{
			Percentages: map[string]float64{s.Version: 100.},
		},
	}).Do()

	if err != nil {
		return err
	}

	return s.AwaitDone(op)
}

// Deploy service configuration files and ensure it is ready to be consumed
// Use defaultName to provide default service name and default service configuration
func (s *Service) Deploy(files []string, defaultName string) (*mgmt.Service, error) {
	var configs []*mgmt.ConfigFile
	var err error
	if len(files) > 0 {
		configs, err = s.CreateConfigFiles(files)
		if err != nil {
			return nil, err
		}
	} else if defaultName != "" {
		swagger := fmt.Sprintf(defaultServiceConfig, defaultName)
		configs = []*mgmt.ConfigFile{&mgmt.ConfigFile{
			FileContents: base64.StdEncoding.EncodeToString([]byte(swagger)),
			FileType:     "OPEN_API_YAML",
			FilePath:     "swagger.yaml",
		}}
	} else {
		return nil, errors.New("Please provide a service configuration file")
	}

	if s.Name == "" && defaultName != "" {
		s.Name = defaultName
	}

	if err = s.Ensure(); err != nil {
		return nil, err
	}

	cfg, err := s.Submit(configs)
	if err != nil {
		return nil, err
	}

	err = s.Rollout()
	if err != nil {
		return nil, err
	}

	// Must be called after submit and rollout
	err = s.Enable(s.Name, s.ProducerProject)
	if err != nil {
		return nil, err
	}

	return cfg, nil
}

// GenerateConfigReport computes the diff between config IDs
func (s *Service) GenerateConfigReport() (*mgmt.GenerateConfigReportResponse, error) {
	// TODO: Union type for newConfig requires @type field
	body := "{" +
		"  \"newConfig\": {" +
		"		 \"@type\": \"type.googleapis.com/google.api.servicemanagement.v1.ConfigRef\"," +
		"	   \"name\": \"services/" + s.Name + "/configs/" + s.Version + "\"" +
		"  }" +
		"}"

	resp, err := s.hc.Post(
		"https://servicemanagement.googleapis.com/v1/services:generateConfigReport",
		"application/json", strings.NewReader(body))
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	op := &mgmt.GenerateConfigReportResponse{}
	err = json.NewDecoder(resp.Body).Decode(op)
	return op, err
}

// Work-around for union types
func remarshal(data interface{}, target interface{}) error {
	bytes, err := json.Marshal(data)
	if err != nil {
		return err
	}
	return json.Unmarshal(bytes, target)
}
