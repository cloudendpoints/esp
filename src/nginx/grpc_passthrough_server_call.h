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
#ifndef NGINX_GRPC_PASSTHROUGH_SERVER_CALL_H_
#define NGINX_GRPC_PASSTHROUGH_SERVER_CALL_H_

#include <map>
#include <string>
#include <vector>

extern "C" {
#include "third_party/nginx/src/http/ngx_http.h"
}

#include "grpc++/support/byte_buffer.h"
#include "include/api_manager/utils/status.h"
#include "src/nginx/grpc_server_call.h"

namespace google {
namespace api_manager {
namespace nginx {

// grpc::ServerCall implementation for gRPC pass-through.
//
// Most of the ::grpc::ServerCall implementation is in the base class -
// NgxEspGrpcServerCall. This class implements ServerCall's Finish() and
// request/response conversion virtual functions defined by
// NgxEspGrpcServerCall.
class NgxEspGrpcPassThroughServerCall : public NgxEspGrpcServerCall {
 public:
  // Creates an instance of NgxEspGrpcPassThroughServerCall. If successful,
  // returns an OK status and out points to the created instance. Otherwise,
  // returns the error status.
  static utils::Status Create(
      ngx_http_request_t* r,
      std::shared_ptr<NgxEspGrpcPassThroughServerCall>* out);

 private:
  // ServerCall::Finish() implementation
  virtual void Finish(
      const utils::Status& status,
      std::multimap<std::string, std::string> response_trailers);

  // NgxEspGrpcServerCall implementation
  virtual bool ConvertRequestBody(std::vector<gpr_slice>* out);
  virtual bool ConvertResponseMessage(const ::grpc::ByteBuffer& msg,
                                      ngx_chain_t* out);
  virtual const ngx_str_t& response_content_type() const;

  // Constructor
  NgxEspGrpcPassThroughServerCall(ngx_http_request_t* r);
};

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // NGINX_GRPC_PASSTHROUGH_SERVER_CALL_H_
