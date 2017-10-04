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
#ifndef SRC_NGINX_GRPC_WEB_SERVER_CALL_H_
#define SRC_NGINX_GRPC_WEB_SERVER_CALL_H_

#include <cstdint>
#include <functional>
#include <map>
#include "grpc++/support/byte_buffer.h"
#include "include/api_manager/utils/status.h"
#include "src/nginx/grpc_passthrough_server_call.h"

namespace google {
namespace api_manager {
namespace nginx {

class NgxEspGrpcWebServerCall
    : public google::api_manager::nginx::NgxEspGrpcPassThroughServerCall {
 public:
  // Creates an instance of NgxEspGrpcWebServerCall. If successful, returns an
  // OK status and out points to the created instance. Otherwise, returns the
  // error status.
  static utils::Status Create(ngx_http_request_t* r,
                              std::shared_ptr<NgxEspGrpcWebServerCall>* out);

  NgxEspGrpcWebServerCall(ngx_http_request_t* r);
  virtual ~NgxEspGrpcWebServerCall();

  // NgxEspGrpcWebServerCall is neither copyable nor movable.
  NgxEspGrpcWebServerCall(const NgxEspGrpcWebServerCall&) = delete;
  NgxEspGrpcWebServerCall& operator=(const NgxEspGrpcWebServerCall&) = delete;

 protected:
  // ServerCall::Finish() implementation
  void Finish(
      const utils::Status& status,
      std::multimap<std::string, std::string> response_trailers) override;

  const ngx_str_t& response_content_type() const override;
};

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // SRC_NGINX_GRPC_WEB_SERVER_CALL_H_
