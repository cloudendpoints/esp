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
//
////////////////////////////////////////////////////////////////////////////////
//
#include "src/nginx/grpc_queue.h"

extern "C" {
#include "ngx_event.h"
}

namespace google {
namespace api_manager {
namespace nginx {

// For the curious, here's the background on why this GRPC queue
// component exists, and how it works:
//
// Nginx is architected as a single-threaded event loop.  Libgrpc
// requires some amount of background processing, and doesn't natively
// understand nginx's event loop.  So there's some conflict.
//
// One way to work around this is to implement libgrpc's grpc_endpoint
// interface with an implementation that handles IO using nginx
// functions.  The ESP team explored this path: it's doable, but winds
// up using a lot of internal libgrpc APIs, and after dealing with
// build issues for a while, the team decided it was untenable.
//
// Fortunately, there's another straightforward approach, using the
// public libgrpc API.  It turns out that nginx is only _mostly_
// single-threaded: because the POSIX API isn't traditionally strong
// when it comes to asynchronous IO, nginx can be run with an IO
// thread pool, which posts callbacks back to the main nginx thread as
// IO completes.  And because nginx is relatively portable, it's able
// to do this with an OS-agnostic interface.
//
// This code runs a separate libgrpc processing thread, pulling events
// from a ::grpc::CompletionQueue and donating cycles to libgrpc in
// the process.  As events are pulled, the thread casts their tags to
// NgxEspGrpcQueue::Tag* objects, and queues them back to the nginx
// main thread, which then calls them.  This has the effect of
// synchronizing those callbacks with the other work being done on the
// main thread.
//
// There's one additional minor problem to be solved, though: the
// nginx interface for posting callbacks back to the main thread --
// ngx_notify() -- takes as a parameter a bare function pointer; no
// associated data.
//
// Since there's only one callback function, that callback function
// must be operating on global data -- so no matter what API we use
// for managing the queue, under the covers, there's a singleton
// somewhere.  So we make it explicit: NgxEspGrpcQueue is the singleton.
//
// NgxEspGrpcQueue::Instance() returns a shared_ptr<> to that singleton
// instance, creating it if necessary (and initializing libgrpc if
// necessary).  A global weak_ptr<> is kept to maintain a pointer to
// that singleton as long as any component has a shared_ptr<>
// reference to it.
//
// When the last shared_ptr<> to the instance is destroyed,
// NgxEspGrpcQueue's destructor will shut down the ::grpc::CompletionQueue,
// and then join with the queue processing thread.  Libgrpc will
// continue supplying events to the thread until the queue is clear;
// then, the queue will return a shutdown event, and the thread will
// exit, allowing the destructor to proceed to completion.
//
// The biggest downside of this approach is that it disables reverse
// debugging.  C'est la vie.
//
// A minor downside of this approach is that it involves making more
// context switches than one might otherwise like.  If this becomes a
// performance problem, it will likely be solved by working with the
// GRPC team to create an API for integrating libgrpc into arbitrary
// event loops.

namespace {
const std::chrono::seconds kShutdownTimeout(2);
}

std::weak_ptr<NgxEspGrpcQueue> NgxEspGrpcQueue::instance;

std::shared_ptr<NgxEspGrpcQueue> NgxEspGrpcQueue::Instance() {
  std::shared_ptr<NgxEspGrpcQueue> result = instance.lock();
  if (!result) {
    result = std::shared_ptr<NgxEspGrpcQueue>(new NgxEspGrpcQueue, &Deleter);
    instance = result;
  }
  return result;
}

std::shared_ptr<NgxEspGrpcQueue> NgxEspGrpcQueue::TryInstance() {
  return instance.lock();
}

void NgxEspGrpcQueue::Init(ngx_cycle_t *cycle) {
  ngx_notify_init(&notify_, NginxTagHandler, cycle);
}

// Runs GRPC event callbacks on the main nginx thread.
void NgxEspGrpcQueue::NginxTagHandler(ngx_event_t *) {
  std::shared_ptr<NgxEspGrpcQueue> queue = TryInstance();
  if (queue) {
    queue->DrainPending();
  }
}

void NgxEspGrpcQueue::WorkerThread(NgxEspGrpcQueue *queue) {
  void *tag;
  bool ok;
  while (true) {
    auto status = queue->cq_->AsyncNext(
        &tag, &ok, std::chrono::system_clock::now() + kShutdownTimeout);
    if (status == ::grpc::CompletionQueue::NextStatus::SHUTDOWN ||
        (status == ::grpc::CompletionQueue::NextStatus::TIMEOUT &&
         queue->shutting_down_)) {
      break;
    }
    if (status == ::grpc::CompletionQueue::NextStatus::TIMEOUT) {
      continue;
    }
    std::unique_ptr<Tag> cb(static_cast<Tag *>(tag));
    if (cb) {
      bool notify_nginx = false;
      {
        std::lock_guard<std::mutex> lock(queue->mu_);
        queue->pending_.emplace_back(Finalizer{std::move(cb), ok});
        if (!queue->notified_) {
          notify_nginx = true;
          queue->notified_ = true;
        }
      }
      if (notify_nginx) {
        ngx_notify(&queue->notify_);
      }
    }
  }
}

void NgxEspGrpcQueue::Deleter(NgxEspGrpcQueue *lib) { delete lib; }

NgxEspGrpcQueue::NgxEspGrpcQueue()
    : cq_(new ::grpc::CompletionQueue()),
      notified_(false),
      shutting_down_(false) {
  worker_thread_ = std::thread(&NgxEspGrpcQueue::WorkerThread, this);
}

NgxEspGrpcQueue::~NgxEspGrpcQueue() {
  // N.B. At this point, we expect that all components have
  // synchronized with the shutdown and are no longer enqueueing new
  // events.
  //
  // There might be rare cases when there are pending callback
  // tags at this point in the shutdown sequence. E.g., the request
  // was finalized due to an error (e.g. transcoding error due to
  // invalid JSON) so NGINX thinks it has processed all requests,
  // but the call to the backend was still in flight and it has
  // returned just now.
  //
  // If this happens, this code handles them correctly, by:
  //
  //   * Shutting down the queue
  //
  //   * Waiting for the queue to drain (i.e. waiting for the event
  //     worker thread to dequeue all pending tags and exit)
  //
  //   * Ignoring the outstanding events as they may try to enqueue
  //     new events, which is dangerous as the completion queue
  //     has been shut down.

  cq_->Shutdown();

  // TODO: This flag is a temporary workaround to force shutdown
  // completing queue. To be removed.
  shutting_down_ = true;

  // N.B. Joining on the worker thread is essential, as that thread
  // maintains a raw pointer to this datastructure.
  worker_thread_.join();
}

void NgxEspGrpcQueue::DrainPending() {
  std::deque<Finalizer> pending;
  {
    std::lock_guard<std::mutex> lock(mu_);
    pending.swap(pending_);
    notified_ = false;
  }
  for (auto &it : pending) {
    (*it.callback)(it.success);
  }
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
