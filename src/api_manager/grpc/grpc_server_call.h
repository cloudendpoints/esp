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
#ifndef API_MANAGER_GRPC_GRPC_SERVER_CALL_H_
#define API_MANAGER_GRPC_GRPC_SERVER_CALL_H_

#include <memory>

#include "src/api_manager/grpc/call_info.h"
#include "src/api_manager/grpc/server.h"
#include "src/api_manager/grpc/server_call.h"

namespace google {
namespace api_manager {
namespace grpc {

// GrpcServerCall uses the GRPC C++ API to implement the ServerCall
// interface.
class GrpcServerCall : public ServerCall,
                       public std::enable_shared_from_this<GrpcServerCall> {
 public:
  explicit GrpcServerCall(std::shared_ptr<Server> server);
  virtual ~GrpcServerCall();

  // Starts the Endpoints server call proxying sequence.
  static void Start(std::shared_ptr<GrpcServerCall> server_call);

  // Accessors for Server to use for accepting calls.
  ::grpc::GenericServerContext *downstream_context() {
    return &downstream_context_;
  }

  ::grpc::GenericServerAsyncReaderWriter *downstream_reader_writer() {
    return &downstream_reader_writer_;
  }

  // ServerCall
  virtual void AddInitialMetadata(std::string key, std::string value);
  virtual void SendInitialMetadata(std::function<void(bool)> continuation);
  virtual void Read(::grpc::ByteBuffer *msg,
                    std::function<void(bool)> continuation);
  virtual void Write(const ::grpc::ByteBuffer &msg,
                     std::function<void(bool)> continuation);
  virtual void Finish(
      const utils::Status &status,
      std::multimap<std::string, std::string> response_trailers);
  virtual void RecordBackendTime(int64_t backend_time) {}

 private:
  std::shared_ptr<GrpcServerCall> GetPtr() { return shared_from_this(); }

  CallInfo *call_info() { return call_info_.get(); }

  void CallAccepted() { call_info_.reset(new CallInfo(&downstream_context_)); }

  void Finish(const utils::Status &status) {
    Finish(status, std::multimap<std::string, std::string>());
  }

  std::shared_ptr<Server> server_;
  ::grpc::GenericServerContext downstream_context_;
  std::unique_ptr<CallInfo> call_info_;
  ::grpc::GenericServerAsyncReaderWriter downstream_reader_writer_{
      &downstream_context_};

  // The CallHandler for this call.  The handler may hold references
  // to the CallInfo passed to it via its Request and Response
  // interfaces.  To guarantee pointer validity, it should be
  // destroyed first.
  std::unique_ptr<CallHandler> call_handler_;
};

}  // namespace grpc
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_GRPC_GRPC_SERVER_CALL_H_
