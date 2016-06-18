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
#ifndef API_MANAGER_GRPC_SERVER_BUILDER_H_
#define API_MANAGER_GRPC_SERVER_BUILDER_H_

#include <functional>
#include <memory>
#include <string>

#include "grpc++/grpc++.h"
#include "src/api_manager/grpc/call_handler.h"

namespace google {
namespace api_manager {
namespace grpc {

class Server;

class ServerBuilder {
 public:
  ServerBuilder() : calls_max_(100), thread_count_(1) {}

  ServerBuilder &AddListeningPort(
      const std::string &address,
      std::shared_ptr<::grpc::ServerCredentials> creds);
  ServerBuilder &SetCallHandlerFactory(
      std::function<std::unique_ptr<CallHandler>()> factory);
  ServerBuilder &SetMaxParallelCalls(unsigned calls_max);
  ServerBuilder &SetThreadCount(unsigned thread_count);
  ServerBuilder &SetBackendOverride(const std::string &address);
  std::shared_ptr<Server> BuildAndRun();

  unsigned GetMaxParallelCalls() const { return calls_max_; }

 private:
  friend class Server;

  unsigned calls_max_;
  unsigned thread_count_;
  ::grpc::ServerBuilder grpc_builder_;
  std::function<std::unique_ptr<CallHandler>()> call_handler_factory_;
  std::string backend_override_;
};

}  // namespace grpc
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_GRPC_SERVER_BUILDER_H_
