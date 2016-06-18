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
#ifndef API_MANAGER_GRPC_STUB_MAP_H_
#define API_MANAGER_GRPC_STUB_MAP_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <grpc++/generic/generic_stub.h>
#include "grpc++/grpc++.h"

namespace google {
namespace api_manager {
namespace grpc {

// StubMap maintains a set of stubs used for calling GRPC backends.
class StubMap {
 public:
  // Gets a generic stub for a backend.
  std::shared_ptr<::grpc::GenericStub> GetStub(const std::string& backend);

 private:
  std::mutex mu_;
  std::unordered_map<std::string, std::shared_ptr<::grpc::GenericStub>> stubs_;
};

}  // namespace grpc
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_GRPC_STUB_MAP_H_
