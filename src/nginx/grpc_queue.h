/*
 * Copyright (C) Extensible Service Proxy Authors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef NGINX_NGX_ESP_GRPC_QUEUE_H_
#define NGINX_NGX_ESP_GRPC_QUEUE_H_

#include <deque>
#include <memory>
#include <thread>

#include <grpc++/grpc++.h>

extern "C" {
#include "ngx_core.h"
#include "ngx_event.h"
}

#include "src/grpc/async_grpc_queue.h"

namespace google {
namespace api_manager {
namespace nginx {

// The nginx-event-loop-based GRPC queue implementation.
class NgxEspGrpcQueue : public AsyncGrpcQueue {
 public:
  // Returns the global library instance, initializing it if necessary
  // and returning an empty pointer on initialization failure.  This
  // call must be externally synchronized -- i.e. it's fine to call
  // this from the main nginx thread, but not from any other thread.
  static std::shared_ptr<NgxEspGrpcQueue> Instance();

  // Returns the global library instance, or an empty pointer if the
  // instance has not been initialized.  This call must be externally
  // synchronized -- i.e. it's fine to call this from the main nginx
  // thread, but not from any other thread.
  static std::shared_ptr<NgxEspGrpcQueue> TryInstance();

  // Constructs a tag for use with the NgxEspGrpcQueue's completion queue.
  // T must be MoveConstructible.
  template <typename T>
  static void *AllocTag(T callback) {
    return static_cast<void *>(new TypedTag<T>(std::move(callback)));
  }

  // Constructs a tag for use with the NgxEspGrpcQueue's completion queue.
  virtual void *MakeTag(std::function<void(bool)> callback) {
    return AllocTag(std::move(callback));
  }

  // The completion queue processed by the library.  Tags queued to
  // this queue must be created by MakeTag or AllocTag.
  virtual ::grpc::CompletionQueue *GetQueue() { return cq_.get(); }

  void Init(ngx_cycle_t *cycle);

 private:
  static std::weak_ptr<NgxEspGrpcQueue> instance;

  // The base class for the tags queued to the completion queue by the
  // ESP components.  Note that for completion queues accessed via the
  // C++ interfaces, all tags must subclass
  // ::grpc::CompletionQueueTag, since the framework will invoke the
  // virtual FinalizeResult method on the tag before returning it.
  class Tag : public ::grpc::CompletionQueueTag {
   public:
    virtual bool FinalizeResult(void **tag, bool *status) { return true; }
    virtual void operator()(bool ok) = 0;
  };

  // Specializes Tag for the continuation being queued to a completion
  // port.  T must be MoveConstructible.
  template <typename T>
  class TypedTag : public Tag {
   public:
    TypedTag(T t) : t_(std::move(t)) {}

    virtual void operator()(bool ok) { t_(ok); }

   private:
    T t_;
  };

  // The type stored in the pending event queue (pending_).  This
  // holds the callback function and the result to pass to that
  // function once the Nginx main thread picks up the pending event.
  struct Finalizer {
    std::unique_ptr<Tag> callback;
    bool success;
  };

  // Runs GRPC callbacks on the main nginx thread.
  static void NginxTagHandler(ngx_event_t *);

  // The GRPC worker thread main routine.  This shuttles events from
  // the GRPC completion queue to the nginx event queue, getting them
  // onto the main nginx thread.
  //
  // Note that the worker thread's lifetime is strictly contained
  // within the lifetime of its associated NgxEspGrpcQueue (the
  // NgxEspGrpcQueue destructor joins on the thread).  This makes it
  // possible to pass the queue to the worker thread via a raw
  // pointer.
  static void WorkerThread(NgxEspGrpcQueue *queue);

  // Deletes the NgxEspGrpcQueue.  (This lets us avoid making the
  // constructor and destructor public, which is a little overly
  // paranoid, but doesn't hurt.)
  static void Deleter(NgxEspGrpcQueue *queue);

  NgxEspGrpcQueue();
  virtual ~NgxEspGrpcQueue();

  // Drains the contents of the pending_ queue.
  void DrainPending();

  std::mutex mu_;
  ngx_event_t notify_;
  std::unique_ptr<::grpc::CompletionQueue> cq_;
  std::deque<Finalizer> pending_;
  bool notified_;
  bool shutting_down_;

  std::thread worker_thread_;
};

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // NGINX_NGX_ESP_GRPC_QUEUE_H_
