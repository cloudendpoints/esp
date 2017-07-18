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
#ifndef GRPC_PROXY_FLOW_H_
#define GRPC_PROXY_FLOW_H_

#include <memory>
#include <mutex>

#include "grpc++/generic/async_generic_service.h"
#include "grpc++/generic/generic_stub.h"
#include "include/api_manager/utils/status.h"
#include "src/grpc/async_grpc_queue.h"
#include "src/grpc/server_call.h"

namespace google {
namespace api_manager {
namespace grpc {

class ProxyFlow {
 public:
  // Invoked when a call is accepted by the server.  This call
  // instantiates an asynchronous ProxyFlow object which handles
  // proxying the GRPC call to an upstream backend server.
  static void Start(AsyncGrpcQueue *async_grpc_queue,
                    std::shared_ptr<ServerCall> server_call,
                    std::shared_ptr<::grpc::GenericStub> upstream_stub,
                    const std::string &method,
                    const std::multimap<std::string, std::string> &headers);

  ProxyFlow(AsyncGrpcQueue *async_grpc_queue,
            std::shared_ptr<ServerCall> server_call,
            std::shared_ptr<::grpc::GenericStub> upstream_stub);
  ~ProxyFlow() {}

 private:
  // Translates GRPC status objects to ESP status objects.
  static utils::Status StatusFromGRPCStatus(const ::grpc::Status &status);

  // The state machine manipulators -- see proxy_flow.cc for details.

  // Common to both paths:
  static void StartUpstreamCall(std::shared_ptr<ProxyFlow> flow,
                                const std::string &method);

  // The downstream->upstream functions:
  static void StartDownstreamReadMessage(std::shared_ptr<ProxyFlow> flow);
  static void StartUpstreamWritesDone(std::shared_ptr<ProxyFlow> flow,
                                      utils::Status status);
  static void StartUpstreamWriteMessage(std::shared_ptr<ProxyFlow> flow,
                                        bool last);

  // The upstream->downstream functions:
  static void StartUpstreamReadInitialMetadata(std::shared_ptr<ProxyFlow> flow);
  static void StartDownstreamWriteInitialMetadata(
      std::shared_ptr<ProxyFlow> flow);
  static void StartUpstreamReadMessage(std::shared_ptr<ProxyFlow> flow);
  static void StartDownstreamWriteMessage(std::shared_ptr<ProxyFlow> flow);
  static void StartUpstreamFinish(std::shared_ptr<ProxyFlow> flow);
  static void StartDownstreamFinish(std::shared_ptr<ProxyFlow> flow,
                                    utils::Status status);

  std::mutex mu_;

  // If true, the downstream side is no longer sending data, and a
  // WritesDone call has been issued to the upstream backend.
  bool sent_upstream_writes_done_;

  // If true, the upstream backend is no longer sending data.
  bool started_upstream_finish_;

  // If true, we've sent a final status to the downstream client.
  bool sent_downstream_finish_;

  AsyncGrpcQueue *async_grpc_queue_;
  std::shared_ptr<ServerCall> server_call_;
  std::shared_ptr<::grpc::GenericStub> upstream_stub_;
  ::grpc::ClientContext upstream_context_;
  std::unique_ptr<::grpc::GenericClientAsyncReaderWriter>
      upstream_reader_writer_;
  utils::Status status_from_esp_;
  ::grpc::Status status_from_upstream_;
  ::grpc::ByteBuffer downstream_to_upstream_buffer_;
  ::grpc::ByteBuffer upstream_to_downstream_buffer_;

  // The backend request start time.
  std::chrono::system_clock::time_point start_time_;
};

}  // namespace grpc
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_GRPC_PROXY_FLOW_H_
