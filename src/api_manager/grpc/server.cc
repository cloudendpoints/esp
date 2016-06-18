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
#include "src/api_manager/grpc/server.h"

#include <cassert>

#include "src/api_manager/grpc/grpc_server_call.h"
#include "src/api_manager/grpc/server_builder.h"

namespace google {
namespace api_manager {
namespace grpc {

Server::Server(ServerBuilder *builder)
    : shutdown_(false),
      accept_in_progress_(false),
      calls_max_(builder->calls_max_),
      calls_in_progress_(0),
      backend_override_(builder->backend_override_),
      queue_(builder->grpc_builder_.AddCompletionQueue()),
      threads_(builder->thread_count_),
      call_handler_factory_(builder->call_handler_factory_) {
  if (!call_handler_factory_) {
    call_handler_factory_ = []() {
      return std::unique_ptr<CallHandler>(new NopCallHandler());
    };
  }
  builder->grpc_builder_.RegisterAsyncGenericService(&service_);
  server_ = builder->grpc_builder_.BuildAndStart();
}

void Server::Start() {
  for (auto &it : threads_) {
    it = std::thread(&Server::RunProxyQueue, GetPtr());
  }
  TryStartCall();
}

void Server::RunProxyQueue(std::shared_ptr<Server> server) {
  void *tag_pointer;
  bool ok;
  while (server->queue_->Next(&tag_pointer, &ok)) {
    Tag *t = reinterpret_cast<Tag *>(tag_pointer);
    (*t)(ok);
    delete t;
  }
}

void Server::StartShutdown() {
  std::lock_guard<std::mutex> lock(mu_);
  StartShutdownLocked();
}

void Server::WaitForShutdown() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!shutdown_) {
      StartShutdownLocked();
    }
  }
  server_->Wait();
  queue_->Shutdown();
  for (auto &it : threads_) {
    if (it.joinable()) {
      it.join();
    }
  }
  std::unique_lock<std::mutex> lock(mu_);
  shutdown_cv_.wait(lock, [this] { return calls_in_progress_ == 0; });
  assert(calls_in_progress_ == 0);
}

void Server::StartShutdownLocked() {
  if (!shutdown_) {
    server_->Shutdown(gpr_time_0(GPR_CLOCK_MONOTONIC));
    shutdown_ = true;
  }
}

void Server::TryStartCall() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (shutdown_ || accept_in_progress_) {
      return;
    }
    if (calls_in_progress_ >= calls_max_) {
      return;
    }
    calls_in_progress_++;
  }

  // Keep this object alive until the call and lambda are complete.
  std::shared_ptr<Server> server = GetPtr();

  auto grpc_server_call = std::make_shared<GrpcServerCall>(server);

  service_.RequestCall(grpc_server_call->downstream_context(),
                       grpc_server_call->downstream_reader_writer(),
                       queue_.get(), queue_.get(),
                       MakeTag([server, grpc_server_call](bool ok) {
                         server->CallAccepted();
                         if (ok) {
                           GrpcServerCall::Start(std::move(grpc_server_call));
                         }
                       }));
}

void Server::CallAccepted() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    accept_in_progress_ = false;
  }
  TryStartCall();
}

void Server::CallComplete() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    calls_in_progress_--;
    if (shutdown_ && !calls_in_progress_) {
      shutdown_cv_.notify_all();
    }
  }
  TryStartCall();
}

}  // namespace grpc
}  // namespace api_manager
}  // namespace google
