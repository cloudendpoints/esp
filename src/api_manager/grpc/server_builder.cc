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
#include "src/api_manager/grpc/server_builder.h"

#include "src/api_manager/grpc/server.h"

namespace google {
namespace api_manager {
namespace grpc {

ServerBuilder &ServerBuilder::AddListeningPort(
    const std::string &address,
    std::shared_ptr<::grpc::ServerCredentials> creds) {
  grpc_builder_.AddListeningPort(address, creds);
  return *this;
}

ServerBuilder &ServerBuilder::SetCallHandlerFactory(
    std::function<std::unique_ptr<CallHandler>()> factory) {
  call_handler_factory_ = factory;
  return *this;
}

ServerBuilder &ServerBuilder::SetBackendOverride(const std::string &address) {
  backend_override_ = address;
  return *this;
}

ServerBuilder &ServerBuilder::SetMaxParallelCalls(unsigned calls_max) {
  calls_max_ = calls_max;
  return *this;
}

ServerBuilder &ServerBuilder::SetThreadCount(unsigned thread_count) {
  thread_count_ = thread_count;
  return *this;
}

std::shared_ptr<Server> ServerBuilder::BuildAndRun() {
  std::shared_ptr<Server> server(new Server(this));
  server->Start();
  return server;
}

}  // namespace grpc
}  // namespace api_manager
}  // namespace google
