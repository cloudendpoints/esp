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
#include "src/nginx/grpc_web_server_call.h"

#include "src/nginx/grpc_web_finish.h"

namespace google {
namespace api_manager {
namespace nginx {
namespace {
const ngx_str_t kContentTypeGrpcWeb = ngx_string("application/grpc-web");
}  // namespace

utils::Status NgxEspGrpcWebServerCall::Create(
    ngx_http_request_t* r, std::shared_ptr<NgxEspGrpcWebServerCall>* out) {
  std::shared_ptr<NgxEspGrpcWebServerCall> call(new NgxEspGrpcWebServerCall(r));
  auto status = call->ProcessPrereadRequestBody();
  if (!status.ok()) {
    return status;
  }
  *out = call;
  return utils::Status::OK;
}

NgxEspGrpcWebServerCall::NgxEspGrpcWebServerCall(ngx_http_request_t* r)
    : NgxEspGrpcPassThroughServerCall(r) {}

NgxEspGrpcWebServerCall::~NgxEspGrpcWebServerCall() {}

void NgxEspGrpcWebServerCall::Finish(
    const utils::Status& status,
    std::multimap<std::string, std::string> response_trailers) {
  if (!cln_.data) {
    return;
  }

  // Make sure the headers have been sent
  if (!r_->header_sent) {
    auto status = WriteDownstreamHeaders();
    if (!status.ok()) {
      ngx_http_finalize_request(r_,
                                GrpcWebFinish(r_, status, response_trailers));
      return;
    }
  }

  ngx_http_finalize_request(r_, GrpcWebFinish(r_, status, response_trailers));
}

const ngx_str_t& NgxEspGrpcWebServerCall::response_content_type() const {
  return kContentTypeGrpcWeb;
}
}  // namespace nginx
}  // namespace api_manager
}  // namespace google
