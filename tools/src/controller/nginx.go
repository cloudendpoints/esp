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
	"bytes"
	"io/ioutil"
	"os"
	"os/exec"

	"github.com/golang/glog"
)

// Nanny nginx process

type Nginx struct {
	ConfigStore

	binPath      string
	confPath     string
	templatePath string

	Conf    *Configuration
	current string
	running bool
}

func MakeNginx(binPath, confPath, templatePath, configPath string) (*Nginx, error) {
	out := &Nginx{
		binPath:      binPath,
		confPath:     confPath,
		templatePath: templatePath,
		Conf:         NewConfig(),
	}

	var err error
	out.ConfigStore, err = MakeConfigStore(configPath, "")

	if err != nil {
		return nil, err
	}

	return out, nil
}

func (n *Nginx) Run() {
	glog.Info("Starting nginx")
	n.writeConfiguration()
	cmd := exec.Command(n.binPath, "-c", n.confPath)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Start(); err != nil {
		glog.Errorf("nginx start error: %v", err)
	}
	n.running = true
}

func (n *Nginx) Reload() error {
	glog.Info("Reload nginx")
	if n.writeConfiguration() {
		cmd := exec.Command(n.binPath, "-s", "reload")
		out, err := cmd.CombinedOutput()
		if err != nil {
			glog.Errorf("nginx reload error: %s", out)
			return err
		}
	}

	return nil
}

func (n *Nginx) Stop() error {
	glog.Info("Quitting nginx")
	cmd := exec.Command(n.binPath, "-s", "quit")
	out, err := cmd.CombinedOutput()
	if err != nil {
		glog.Errorf("nginx quit error: %s", out)
	}
	return err
}

// Check that configuration changed and return true if reload is needed
func (n *Nginx) writeConfiguration() bool {
	var bytes bytes.Buffer
	err := n.Conf.WriteTemplate(n.templatePath, &bytes)
	if err != nil {
		glog.Errorf("Failed to write template: %s", err.Error())
		return false
	}

	var content = bytes.String()
	if content == n.current {
		glog.Info("Configuration is identical, skipping reload")
		return false
	} else {
		n.current = content
	}

	glog.Info(content)
	err = ioutil.WriteFile(n.confPath, []byte(content), 0644)
	if err != nil {
		glog.Warningf("Failed to write configuration file: %s", err.Error())
		return false
	}

	return true
}
