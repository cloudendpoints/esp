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
	"io/ioutil"
	"log"
	"net/http"
	"strings"
	"time"

	mgmt "google.golang.org/api/servicemanagement/v1"

	"golang.org/x/net/context"
	"golang.org/x/oauth2/google"
)

type Service struct {
	Name            string
	Version         string
	CredentialsFile string

	ProducerProject string

	hc  *http.Client
	api *mgmt.APIService
}

type ConfigType int

const (
	OpenApiJson            ConfigType = 1
	OpenApiYaml            ConfigType = 2
	ServiceConfigYaml      ConfigType = 3
	FileDescriptorSetProto ConfigType = 4
)

func (s *Service) Init() (err error) {
	ctx := context.Background()

	if s.CredentialsFile != "" {
		json, err := ioutil.ReadFile(s.CredentialsFile)
		if err != nil {
			return err
		}
		config, err := google.JWTConfigFromJSON(json, mgmt.ServiceManagementScope)
		if err != nil {
			return err
		}
		s.hc = config.Client(ctx)
	} else {
		s.hc, err = google.DefaultClient(ctx, mgmt.ServiceManagementScope)
		if err != nil {
			return
		}
	}

	s.api, err = mgmt.New(s.hc)
	if err != nil {
		return
	}

	return nil
}

func (s *Service) Fetch() (svc *mgmt.Service, err error) {
	if s.Name == "" {
		err = errors.New("Missing service name")
	} else if s.Version == "" {
		svc, err = s.api.Services.GetConfig(s.Name).Do()
		if err == nil {
			log.Println("Service version is", svc.Id)
			s.Version = svc.Id
		}
	} else {
		svc, err = s.api.Services.Configs.Get(s.Name, s.Version).Do()
	}
	return
}

// Wait for completion of the operation
func (s *Service) Await(op *mgmt.Operation) *mgmt.Operation {
	try := 0
	delay := time.Second
	MaxTries := 10

	var err error
	for !op.Done && try < MaxTries {
		if try > 0 {
			log.Println("Sleeping before the next attempt:", delay)
			time.Sleep(delay)
			delay = 2 * delay
			log.Println("Repeat attempt #", try+1)
		}
		log.Println("Retrieving operation status:", op.Name)
		op, err = s.api.Operations.Get(op.Name).Do()
		if err != nil {
			log.Println("Failed to retrieve the operation status", err)
			return op
		}
		try = try + 1
	}

	return op
}

func (s *Service) AwaitDone(op *mgmt.Operation) error {
	op = s.Await(op)
	if op.Done {
		return nil
	} else {
		return errors.New("Failed to complete operation: " + op.Name)
	}
}

func (s *Service) Exists() bool {
	_, err := s.api.Services.Get(s.Name).Do()
	return err == nil
}

func (s *Service) Create() error {
	log.Println("Create service", s.Name)
	op, err := s.api.Services.Create(&mgmt.ManagedService{
		ServiceName:       s.Name,
		ProducerProjectId: s.ProducerProject,
	}).Do()
	if err != nil {
		return err
	}
	return s.AwaitDone(op)
}

func (s *Service) Enable(consumerProject string) error {
	log.Println("Enable service", s.Name)
	op, err := s.api.Services.Enable(s.Name, &mgmt.EnableServiceRequest{
		ConsumerId: "project:" + consumerProject,
	}).Do()
	if err != nil {
		return err
	}
	return s.AwaitDone(op)
}

func (s *Service) Delete() error {
	log.Println("Delete service", s.Name)
	op, err := s.api.Services.Delete(s.Name).Do()
	if err != nil {
		return err
	}
	return s.AwaitDone(op)
}

func MakeConfigFiles(serviceAPI,
	serviceConfig, serviceProto []string) (map[string]ConfigType, error) {
	out := make(map[string]ConfigType)
	for _, f := range serviceAPI {
		if strings.HasSuffix(strings.ToUpper(f), ".YAML") {
			out[f] = OpenApiYaml
		} else if strings.HasSuffix(strings.ToUpper(f), ".JSON") {
			out[f] = OpenApiJson
		} else {
			return nil, errors.New("Cannot determine config file type of " + f)
		}
	}

	for _, f := range serviceConfig {
		out[f] = ServiceConfigYaml
	}

	for _, f := range serviceProto {
		out[f] = FileDescriptorSetProto
	}

	return out, nil
}

/** Submit creates a service config from the interface descriptions */
func (s *Service) Submit(files map[string]ConfigType) (*mgmt.Service, error) {
	log.Println("Submit service configuration for", s.Name)
	configFiles := make([]*mgmt.ConfigFile, 0)

	for filePath, configType := range files {
		var fileType string
		switch configType {
		case OpenApiJson:
			fileType = "OPEN_API_JSON"
		case OpenApiYaml:
			fileType = "OPEN_API_YAML"
		case ServiceConfigYaml:
			fileType = "SERVICE_CONFIG_YAML"
		case FileDescriptorSetProto:
			fileType = "FILE_DESCRIPTOR_SET_PROTO"
		}

		contents, err := ioutil.ReadFile(filePath)
		if err != nil {
			return nil, errors.New("Cannot read file " + filePath)
		}

		configFiles = append(configFiles, &mgmt.ConfigFile{
			FileContents: base64.StdEncoding.EncodeToString(contents),
			FileType:     fileType,
			FilePath:     filePath,
		})
	}

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

func (s *Service) GenerateConfigReport() (*mgmt.GenerateConfigReportResponse, error) {
	return s.api.Services.GenerateConfigReport(&mgmt.GenerateConfigReportRequest{
		NewConfig: mgmt.GenerateConfigReportRequestNewConfig(&mgmt.ConfigRef{
			Name: "services/" + s.Name + "/configs/" + s.Version,
		}),
	}).Do()
}

func (s *Service) Rollout() error {
	log.Println("Rollout service " + s.Name + " config " + s.Version)

	// API is broken upstream. This code simulates the following using raw
	// HTTP Client:

	// req := s.api.Services.Rollouts.Create(s.Name, &mgmt.Rollout{
	//	TrafficPercentStrategy: &mgmt.TrafficPercentStrategy{
	//		Percentages: map[string]int{s.Version: 100} ,
	//	},
	// })

	body := "{" +
		"\"serviceName\": \"" + s.Name + "\"," +
		"\"trafficPercentStrategy\": {" +
		"  \"percentages\": {" +
		"    \"" + s.Version + "\": 100," +
		"}}}"

	resp, err := s.hc.Post(
		"https://servicemanagement.googleapis.com/v1/services/"+s.Name+"/rollouts",
		"application/json", strings.NewReader(body))

	if err != nil {
		return err
	}

	defer resp.Body.Close()
	op := &mgmt.Operation{}
	err = json.NewDecoder(resp.Body).Decode(op)
	if err != nil {
		return err
	}

	return s.AwaitDone(op)
}

// Work-around for union types
func remarshal(data interface{}, target interface{}) error {
	bytes, err := json.Marshal(data)
	if err != nil {
		return err
	}
	return json.Unmarshal(bytes, target)
}
