/*
 * Copyright (C) Endpoints Server Proxy Authors
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
#ifndef API_MANAGER_GRPC_SERVER_H_
#define API_MANAGER_GRPC_SERVER_H_

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "grpc++/generic/async_generic_service.h"
#include "grpc++/grpc++.h"
#include "include/api_manager/async_grpc_queue.h"
#include "src/api_manager/grpc/call_handler.h"
#include "src/api_manager/grpc/stub_map.h"

namespace google {
namespace api_manager {
namespace grpc {

class ServerBuilder;

class Server : public AsyncGrpcQueue,
               public std::enable_shared_from_this<Server> {
 public:
  std::shared_ptr<Server> GetPtr() { return shared_from_this(); }

  // Starts the shutdown sequence for this server.  It's not necessary
  // to call this before invoking the destructor, but Shutdown may
  // take longer if it needs to synchronously shut down the server.
  void StartShutdown();

  // Waits for the server to be shutdown (invoking StartShutdown if
  // that method has not yet been invoked).
  void WaitForShutdown();

  // Destructs the Server.  This should only be invoked by
  // std::shared_ptr, when the last reference to the Server is
  // deleted.
  ~Server() {}

  // Builds a tag suitable for use with the server's completion queue.
  virtual void *MakeTag(std::function<void(bool)> continuation) {
    return reinterpret_cast<void *>(new Tag(continuation));
  }

  virtual ::grpc::CompletionQueue *GetQueue() { return queue_.get(); }

  StubMap *stub_map() { return &stub_map_; }

 private:
  friend class GrpcServerCall;
  friend class ServerBuilder;

  // The type of tags queued to the GRPC completion queue maintained
  // by the server.
  typedef std::function<void(bool)> Tag;

  Server(ServerBuilder *builder);

  // Starts the server thread's main loop.  Note that this is done
  // outside of initialization because the thread maintains a
  // reference to the Server; before that reference is allocated and
  // handed off, we want to ensure that the server builder owns an
  // independent reference.
  void Start();

  // The server thread's main loop.
  static void RunProxyQueue(std::shared_ptr<Server> server);

  void CallAccepted();  // Invoked when a GRPC call is accepted.
  void CallComplete();  // Invoked on call destruction.
  void TryStartCall();
  void StartShutdownLocked();

  std::mutex mu_;
  std::condition_variable shutdown_cv_;
  bool shutdown_;
  bool accept_in_progress_;
  unsigned calls_max_;
  unsigned calls_in_progress_;
  std::string backend_override_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<::grpc::ServerCompletionQueue> queue_;
  ::grpc::AsyncGenericService service_;
  std::vector<std::thread> threads_;
  std::function<std::unique_ptr<CallHandler>()> call_handler_factory_;
  StubMap stub_map_;
};

}  // namespace grpc
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_GRPC_SERVER_H_
