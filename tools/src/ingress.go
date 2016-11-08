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
// ESP Ingress controller
//
package main

import (
	"controller"
	"flag"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/golang/glog"

	"k8s.io/client-go/1.5/kubernetes"
	"k8s.io/client-go/1.5/pkg/api"
	"k8s.io/client-go/1.5/rest"
	"k8s.io/client-go/1.5/tools/clientcmd"
)

var (
	namespace     string
	inCluster     bool
	resyncSeconds int
)

func check(err error) {
	if err != nil {
		glog.Errorf("Error at startup: %v", err)
		os.Exit(-1)
	}
}

func main() {
	flag.StringVar(&namespace, "namespace", api.NamespaceAll, "Specify namespace to watch for ingresses")
	flag.BoolVar(&inCluster, "cluster", true, "True if run inside the cluster")
	flag.IntVar(&resyncSeconds, "resync", 5, "Resync period for controllers")
	flag.Parse()

	var (
		config *rest.Config
		err    error
	)

	if inCluster {
		config, err = rest.InClusterConfig()
	} else {
		config, err = clientcmd.NewNonInteractiveDeferredLoadingClientConfig(
			clientcmd.NewDefaultClientConfigLoadingRules(),
			&clientcmd.ConfigOverrides{},
		).ClientConfig()
	}
	check(err)
	clientset, err := kubernetes.NewForConfig(config)
	check(err)

	nginx, err := controller.MakeNginx(
		"/usr/sbin/nginx",
		"/etc/nginx/nginx.conf",
		"/etc/nginx/nginx.tmpl",
		"/etc/nginx/endpoints")
	nginx.Conf.PID = "/var/run/nginx.pid"
	// TODO remove after the fix to multiple worker issue
	nginx.Conf.NumWorkerProcesses = 1
	check(err)

	ctl, _ := controller.NewController(nginx, clientset, namespace,
		time.Duration(resyncSeconds)*time.Second)
	stop := make(chan struct{})
	go handleSignals(stop)
	ctl.Run(stop)
}

func handleSignals(stop chan struct{}) {
	signalChan := make(chan os.Signal, 1)
	signal.Notify(signalChan, syscall.SIGTERM)
	signal.Notify(signalChan, syscall.SIGINT)
	<-signalChan
	glog.Info("Caught signal, terminating")
	close(stop)
	glog.Flush()
}
