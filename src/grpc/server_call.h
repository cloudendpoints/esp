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
#ifndef GRPC_SERVER_CALL_H_
#define GRPC_SERVER_CALL_H_

#include <functional>

#include <grpc++/grpc++.h>

#include "include/api_manager/utils/status.h"

namespace google {
namespace api_manager {
namespace grpc {

// ServerCall is the interface used for proxying a downstream GRPC
// call.
class ServerCall {
 public:
  virtual ~ServerCall() {}

  // GRPC protocol operations on the downstream GRPC call.
  virtual void SendInitialMetadata(
      std::multimap<std::string, std::string> initial_metadata,
      std::function<void(bool)> continuation) = 0;

  // Continuation receives an indicator (true to continue, false to interrupt)
  // and an optional error status
  virtual void Read(::grpc::ByteBuffer *msg,
                    std::function<void(bool, utils::Status)> continuation) = 0;

  virtual void Write(const ::grpc::ByteBuffer &msg,
                     std::function<void(bool)> continuation) = 0;
  virtual void Finish(
      const utils::Status &status,
      std::multimap<std::string, std::string> response_trailers) = 0;
  virtual void RecordBackendTime(int64_t backend_time) = 0;
};

}  // namespace grpc
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_GRPC_SERVER_CALL_H_
