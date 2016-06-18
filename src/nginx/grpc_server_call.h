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
#ifndef NGINX_NGX_ESP_GRPC_SERVER_CALL_H_
#define NGINX_NGX_ESP_GRPC_SERVER_CALL_H_

extern "C" {
#include "third_party/nginx/src/http/ngx_http.h"
}

#include "src/api_manager/grpc/server_call.h"

namespace google {
namespace api_manager {
namespace nginx {

// Implements the ServerCall interface in a way that wraps a
// downstream connection that's managed by an Nginx HTTP server.
//
// Note: The implementation assumes that all method calls are
// externally synchronized, for example, by running them all on the
// main nginx thread.  The intention is that NgxEspGrpcQueue will be
// used to do this.
class NgxEspGrpcServerCall : public grpc::ServerCall {
 public:
  // Construct an NgxEspGrpcServerCall.
  //
  // Note: as part of the construction sequence, a cleanup handler
  // will be registered on the request, so that if the request is
  // finalized/terminated the NgxEspGrpcServerCall will stop using it.
  NgxEspGrpcServerCall(ngx_http_request_t* r);
  virtual ~NgxEspGrpcServerCall();

  // ServerCall methods.
  virtual void AddInitialMetadata(std::string key, std::string value);
  virtual void SendInitialMetadata(std::function<void(bool)> continuation);
  virtual void Read(::grpc::ByteBuffer* msg,
                    std::function<void(bool)> continuation);
  virtual void Write(const ::grpc::ByteBuffer& msg,
                     std::function<void(bool)> continuation);
  virtual void Finish(
      const utils::Status& status,
      std::multimap<std::string, std::string> response_trailers);
  virtual void RecordBackendTime(int64_t backend_time);

 private:
  static void OnDownstreamPreread(ngx_http_request_t* r);
  static void OnDownstreamReadable(ngx_http_request_t* r);
  static void OnDownstreamWriteable(ngx_http_request_t* r);

  void CompletePendingRead(bool ok);

  void RunPendingRead();

  void ReadDownstreamRequestBody();

  // Attempts to read a GRPC message from downstream into read_msg_;
  // calls CompletePendingRead and returns true if successful.
  bool TryReadDownstreamMessage();

  // Indicates that the request is going away (being freed, &c).  This
  // causes currently outstanding and newly initiated operations to be
  // completed with 'false'.
  static void Cleanup(void* server_call_ptr);

  ngx_http_request_t* r_;
  ngx_http_cleanup_t cln_;
  bool add_header_failed_;
  bool reading_;
  std::function<void(bool)> write_continuation_;
  std::function<void(bool)> read_continuation_;
  ::grpc::ByteBuffer* read_msg_;
  ::std::vector<gpr_slice> downstream_slices_;
};

// Sends Grpc status and response_trailers to client.
ngx_int_t GrpcFinish(ngx_http_request_t* r, const utils::Status& status,
                     std::multimap<std::string, std::string> response_trailers);

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // NGINX_NGX_ESP_GRPC_SERVER_CALL_H_
