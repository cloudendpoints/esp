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
	"errors"
	"reflect"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/golang/glog"

	"k8s.io/client-go/1.5/kubernetes"
	"k8s.io/client-go/1.5/pkg/api"
	"k8s.io/client-go/1.5/pkg/api/v1"
	extensions "k8s.io/client-go/1.5/pkg/apis/extensions/v1beta1"
	"k8s.io/client-go/1.5/pkg/runtime"
	"k8s.io/client-go/1.5/pkg/util/intstr"
	"k8s.io/client-go/1.5/pkg/watch"
	"k8s.io/client-go/1.5/tools/cache"
)

type Controller struct {
	storeIngress  cache.Store
	storeSecret   cache.Store
	storeEndpoint cache.Store
	storeService  cache.Store
	ctlIngress    *cache.Controller
	ctlSecret     *cache.Controller
	ctlEndpoint   *cache.Controller
	ctlService    *cache.Controller
	sync          WorkQueue
	nginx         *Nginx
}

func NewController(
	nginx *Nginx,
	c *kubernetes.Clientset,
	namespace string,
	resyncPeriod time.Duration) (*Controller, error) {
	glog.Infof("Watching resources in namespace: %s", namespace)
	ctl := &Controller{nginx: nginx}
	ctl.sync = makeQueue(ctl.Sync)

	// TODO: add POD IP to ingress status
	ctl.storeIngress, ctl.ctlIngress = cache.NewInformer(
		&cache.ListWatch{
			ListFunc: func(options api.ListOptions) (runtime.Object, error) {
				return c.Extensions().Ingresses(namespace).List(options)
			},
			WatchFunc: func(options api.ListOptions) (watch.Interface, error) {
				return c.Extensions().Ingresses(namespace).Watch(options)
			},
		},
		&extensions.Ingress{},
		resyncPeriod,
		cache.ResourceEventHandlerFuncs{
			AddFunc: func(obj interface{}) {
				ing := obj.(*extensions.Ingress)
				if IsESPIngress(ing) {
					glog.Info("Added ingress ", ing.Name)
					ctl.sync.Enqueue(ing)
				}
			},
			UpdateFunc: func(old, cur interface{}) {
				if !reflect.DeepEqual(old, cur) {
					ingOld := old.(*extensions.Ingress)
					ingCur := cur.(*extensions.Ingress)
					if IsESPIngress(ingOld) && IsESPIngress(ingCur) {
						ctl.sync.Enqueue(ingCur)
					} else if IsESPIngress(ingOld) {
						ctl.sync.Enqueue(ingOld)
					} else if IsESPIngress(ingCur) {
						ctl.sync.Enqueue(ingCur)
					}
				}
			},
			DeleteFunc: func(obj interface{}) {
				ing := obj.(*extensions.Ingress)
				if IsESPIngress(ing) {
					ctl.sync.Enqueue(ing)
				}
			},
		},
	)

	ctl.storeSecret, ctl.ctlSecret = cache.NewInformer(
		&cache.ListWatch{
			ListFunc: func(options api.ListOptions) (runtime.Object, error) {
				return c.Secrets(namespace).List(options)
			},
			WatchFunc: func(options api.ListOptions) (watch.Interface, error) {
				return c.Secrets(namespace).Watch(options)
			},
		},
		&v1.Secret{},
		resyncPeriod,
		cache.ResourceEventHandlerFuncs{
			AddFunc: func(obj interface{}) {
				s := obj.(*v1.Secret)
				if ctl.ReferencesSecret(s) {
					ctl.sync.Enqueue(obj)
				}
			},
			UpdateFunc: func(old, obj interface{}) {
				if !reflect.DeepEqual(old, obj) {
					s := obj.(*v1.Secret)
					if ctl.ReferencesSecret(s) {
						ctl.sync.Enqueue(obj)
					}
				}
			},
			DeleteFunc: func(obj interface{}) {
				s := obj.(*v1.Secret)
				if ctl.ReferencesSecret(s) {
					ctl.sync.Enqueue(obj)
				}
			},
		},
	)

	ctl.storeEndpoint, ctl.ctlEndpoint = cache.NewInformer(
		&cache.ListWatch{
			ListFunc: func(options api.ListOptions) (runtime.Object, error) {
				return c.Endpoints(namespace).List(options)
			},
			WatchFunc: func(options api.ListOptions) (watch.Interface, error) {
				return c.Endpoints(namespace).Watch(options)
			},
		},
		&v1.Endpoints{},
		resyncPeriod,
		cache.ResourceEventHandlerFuncs{
			AddFunc: func(obj interface{}) {
				e := obj.(*v1.Endpoints)
				if ctl.ReferencesService(e.Namespace, e.Name) {
					ctl.sync.Enqueue(obj)
				}
			},
			UpdateFunc: func(old, obj interface{}) {
				if !reflect.DeepEqual(old, obj) {
					e := obj.(*v1.Endpoints)
					if ctl.ReferencesService(e.Namespace, e.Name) {
						ctl.sync.Enqueue(obj)
					}
				}
			},
			DeleteFunc: func(obj interface{}) {
				e := obj.(*v1.Endpoints)
				if ctl.ReferencesService(e.Namespace, e.Name) {
					ctl.sync.Enqueue(obj)
				}
			},
		},
	)

	ctl.storeService, ctl.ctlService = cache.NewInformer(
		&cache.ListWatch{
			ListFunc: func(options api.ListOptions) (runtime.Object, error) {
				return c.Services(namespace).List(options)
			},
			WatchFunc: func(options api.ListOptions) (watch.Interface, error) {
				return c.Services(namespace).Watch(options)
			},
		},
		&v1.Service{},
		resyncPeriod,
		cache.ResourceEventHandlerFuncs{
			AddFunc: func(obj interface{}) {
				s := obj.(*v1.Service)
				if ctl.ReferencesService(s.Namespace, s.Name) {
					ctl.sync.Enqueue(obj)
				}
			},
			UpdateFunc: func(old, obj interface{}) {
				if !reflect.DeepEqual(old, obj) {
					s := obj.(*v1.Service)
					if ctl.ReferencesService(s.Namespace, s.Name) {
						ctl.sync.Enqueue(obj)
					}
				}
			},
			DeleteFunc: func(obj interface{}) {
				s := obj.(*v1.Service)
				if ctl.ReferencesService(s.Namespace, s.Name) {
					ctl.sync.Enqueue(obj)
				}
			},
		},
	)

	return ctl, nil
}

func (ctl *Controller) Run(stop chan struct{}) {
	ctl.nginx.Run()
	go ctl.sync.Run(stop)
	go ctl.ctlIngress.Run(stop)
	go ctl.ctlSecret.Run(stop)
	go ctl.ctlEndpoint.Run(stop)
	go ctl.ctlService.Run(stop)
	<-stop
	ctl.nginx.Stop()
	// TODO remove POD IP from ingress status cleanly
}

// Sync creates a configuration and submits it to nginx
func (ctl *Controller) Sync(obj interface{}) error {
	if !ctl.HasSynced() {
		return errors.New("Deferring sync till controllers have synced")
	}

	glog.Info("Sync caused by ", reflect.TypeOf(obj))

	conf := ctl.nginx.Conf
	conf.Upstreams = make([]*Upstream, 0)
	conf.Servers = make([]*Server, 0)
	var defaultLocation *Location = nil

	for _, ing := range ctl.GetIngresses() {
		// skip if cannot get service config
		name, configId, err := GetServiceConfig(ing)
		if err != nil {
			glog.Warningf("Failed to obtain the service config name and ID, skipping: %v", err)
			continue
		}
		serviceConfig, err := ctl.nginx.GetConfig(name, configId)
		if err != nil {
			glog.Warningf("Failed to retrieve the service config, skipping: %v", err)
			continue
		}

		// use last default backend as implicit "/" rule
		if ing.Spec.Backend != nil {
			if ing.Spec.Backend.ServicePort.Type == intstr.String {
				glog.Warningf("Default backend uses a named port, skipping: %v", ing.Spec.Backend)
			} else {
				if defaultLocation != nil {
					glog.Warningf("Overriding the default upstream %v", defaultLocation.Upstream.Label())
				}

				defaultLocation = &Location{
					Upstream: &Upstream{
						Name:      ing.Spec.Backend.ServiceName,
						Namespace: ing.Namespace,
						Port:      ing.Spec.Backend.ServicePort.IntValue(),
						Protocol:  GetBackendProtocol(ing),
					},
					ServiceConfigFile: serviceConfig,
					StripPrefix:       GetStripPrefix(ing),
				}
			}
		}

		for _, rule := range ing.Spec.Rules {
			server := &Server{
				Name:      rule.Host,
				Locations: make([]*Location, 0),
				Ports: &Ports{
					HTTP: 80,
				},
			}

			if rule.HTTP == nil {
				glog.Warningf("Skipping the ingress rule, missing an HTTP section %v", rule)
				continue
			}

			for _, path := range rule.HTTP.Paths {
				if path.Backend.ServicePort.Type == intstr.String {
					glog.Warningf("Rule backend uses a named port, skipping  %v", path.Backend)
					continue
				}

				upstream := &Upstream{
					Name:      path.Backend.ServiceName,
					Namespace: ing.Namespace,
					Port:      path.Backend.ServicePort.IntValue(),
					Protocol:  GetBackendProtocol(ing),
				}

				location := &Location{
					Path:              path.Path,
					Upstream:          upstream,
					ServiceConfigFile: serviceConfig,
					StripPrefix:       GetStripPrefix(ing),
				}

				// valid paths are: "", "/v1"
				// hence, we need to strip trailing slashes
				for {
					if strings.HasSuffix(location.Path, "/") {
						location.Path = strings.TrimSuffix(location.Path, "/")
					} else {
						break
					}
				}

				conf.Upstreams = append(conf.Upstreams, upstream)
				server.Locations = append(server.Locations, location)
			}

			conf.Servers = append(conf.Servers, server)
		}
	}

	if defaultLocation != nil {
		defaultServer := &Server{
			Name:      "",
			Locations: []*Location{defaultLocation},
			Ports: &Ports{
				HTTP: 80,
			},
		}

		conf.Upstreams = append(conf.Upstreams, defaultLocation.Upstream)
		conf.Servers = append(conf.Servers, defaultServer)
	}

	conf.DeduplicateUpstreams()
	ctl.ResolveUpstreams(conf)
	conf.RemoveEmptyUpstreams()
	conf.RemoveStaleLocations()
	ctl.nginx.Reload()

	return nil
}

// HasSynced returns true after the initial state synchronization
func (ctl *Controller) HasSynced() bool {
	return ctl.ctlIngress.HasSynced() &&
		ctl.ctlSecret.HasSynced() &&
		ctl.ctlEndpoint.HasSynced() &&
		ctl.ctlService.HasSynced()
}

// GetIngresses lists ESP ingress resources sorted by their creation time
func (ctl *Controller) GetIngresses() []*extensions.Ingress {
	ings := make([]*extensions.Ingress, 0)
	for _, i := range ctl.storeIngress.List() {
		ing := i.(*extensions.Ingress)
		if IsESPIngress(ing) {
			ings = append(ings, ing)
		}
	}

	sort.Sort(ByTime(ings))
	return ings
}

// key function used internally by kubernetes (here, namespace is non-empty)
func keyFunc(namespace, name string) string {
	return namespace + "/" + name
}

// --------------------
// Endpoints resolution
// --------------------

func (ctl *Controller) ResolveUpstreams(conf *Configuration) {
	for _, upstream := range conf.Upstreams {
		ctl.ResolveUpstream(upstream)
	}
}

func (ctl *Controller) ResolveUpstream(up *Upstream) {
	svcObj, svcExists, err := ctl.storeService.GetByKey(keyFunc(up.Namespace, up.Name))

	if err != nil || !svcExists {
		glog.Warningf("Cannot resolve upstream %v", up.Label())
		return
	}

	svc := svcObj.(*v1.Service)
	for _, port := range svc.Spec.Ports {
		if int(port.Port) == up.Port || port.TargetPort.String() == strconv.Itoa(up.Port) {
			if port.TargetPort.Type == intstr.String {
				glog.Warningf("Cannot resolve named target port: %s", port.TargetPort.StrVal)
				return
			}
			up.Endpoints = ctl.ResolveEndpoints(svc, port.TargetPort.IntVal)
			return
		}
	}

	glog.Warningf("Failed to find matching service name/port for %v", up.Label())
}

func (ctl *Controller) ResolveEndpoints(svc *v1.Service, port int32) []*Backend {
	out := make([]*Backend, 0)

	for _, m := range ctl.storeEndpoint.List() {
		ep := *m.(*v1.Endpoints)
		if svc.Name == ep.Name && svc.Namespace == ep.Namespace {
			for _, ss := range ep.Subsets {
				for _, epPort := range ss.Ports {
					if !reflect.DeepEqual(epPort.Protocol, v1.ProtocolTCP) {
						continue
					}

					if epPort.Port != port {
						continue
					}

					for _, epAddress := range ss.Addresses {
						out = append(out, &Backend{
							Address: epAddress.IP,
							Port:    int(port),
						})
					}
				}
			}
		}
	}

	sort.Sort(ByAddress(out))
	return out
}

func (ctl *Controller) ReferencesService(namespace, name string) bool {
	for _, i := range ctl.storeIngress.List() {
		ing := i.(*extensions.Ingress)
		if IsESPIngress(ing) && ing.Namespace == namespace {
			if ing.Spec.Backend != nil && ing.Spec.Backend.ServiceName == name {
				return true
			}
			for _, rule := range ing.Spec.Rules {
				if rule.HTTP != nil {
					for _, path := range rule.HTTP.Paths {
						if path.Backend.ServiceName == name {
							return true
						}
					}
				}
			}
		}
	}
	return false
}

// -----------------
// Secret management
// -----------------

func (ctl *Controller) GetTLSData(namespace, name string) (string, string, error) {
	key := keyFunc(namespace, name)
	data, exists, err := ctl.storeSecret.GetByKey(key)
	if err != nil || !exists {
		return "", "", errors.New("Cannot find secret " + key)
	}

	secret := data.(*v1.Secret)
	certData, ok := secret.Data[api.TLSCertKey]
	if !ok {
		return "", "", errors.New("Secret missing data " + key)
	}
	keyData, ok := secret.Data[api.TLSPrivateKeyKey]
	if !ok {
		return "", "", errors.New("Secret missing data " + key)
	}

	return string(certData), string(keyData), nil
}

func (ctl *Controller) ReferencesSecret(s *v1.Secret) bool {
	for _, i := range ctl.storeIngress.List() {
		ing := i.(*extensions.Ingress)
		if IsESPIngress(ing) {
			for _, tls := range ing.Spec.TLS {
				if tls.SecretName == s.Name && ing.Namespace == s.Namespace {
					return true
				}
			}
		}
	}
	return false
}
