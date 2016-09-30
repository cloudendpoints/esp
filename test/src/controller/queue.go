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
	"sync"
	"time"

	"github.com/golang/glog"

	"k8s.io/client-go/1.5/pkg/util/flowcontrol"
)

// A queue of work tickets processed using rate-limiting loop
type WorkQueue interface {
	Enqueue(interface{})
	Run(chan struct{})
}

type WorkQueueImpl struct {
	queue   []interface{}
	process func(interface{}) error
	lock    sync.Mutex
	closing bool
}

func makeQueue(process func(interface{}) error) WorkQueue {
	return &WorkQueueImpl{
		queue:   make([]interface{}, 0),
		closing: false,
		lock:    sync.Mutex{},
		process: process,
	}
}

func (q *WorkQueueImpl) Enqueue(item interface{}) {
	q.lock.Lock()
	if !q.closing {
		q.queue = append(q.queue, item)
	}
	q.lock.Unlock()

}

func (q *WorkQueueImpl) Run(stop chan struct{}) {
	go func() {
		<-stop
		q.lock.Lock()
		q.closing = true
		q.lock.Unlock()
	}()

	rateLimiter := flowcontrol.NewTokenBucketRateLimiter(float32(10), 100)
	var item interface{}
	for {
		rateLimiter.Accept()

		q.lock.Lock()
		if q.closing {
			q.lock.Unlock()
			return
		} else if len(q.queue) == 0 {
			q.lock.Unlock()
		} else {
			item, q.queue = q.queue[0], q.queue[1:]
			q.lock.Unlock()

			for {
				err := q.process(item)
				if err != nil {
					glog.Info("Work item failed, repeating after delay:", err)
					time.Sleep(1 * time.Second)
				} else {
					break
				}
			}
		}
	}
}
