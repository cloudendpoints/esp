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
#ifndef NGINX_NGX_ESP_REQUEST_H_
#define NGINX_NGX_ESP_REQUEST_H_

extern "C" {
#include "src/http/ngx_http.h"
}

#include "include/api_manager/request.h"

namespace google {
namespace api_manager {
namespace nginx {

// Wraps ngx_http_request_t as a ::google::api_manager::Request.
class NgxEspRequest : public Request {
 public:
  NgxEspRequest(ngx_http_request_t *r);
  ~NgxEspRequest();

  virtual std::string GetRequestHTTPMethod();
  virtual std::string GetQueryParameters();
  virtual protocol::Protocol GetFrontendProtocol();
  virtual protocol::Protocol GetBackendProtocol();
  virtual std::string GetRequestPath();
  virtual std::string GetUnparsedRequestPath();
  virtual std::string GetClientIP();

  virtual int64_t GetGrpcRequestBytes();
  virtual int64_t GetGrpcResponseBytes();
  virtual int64_t GetGrpcRequestMessageCounts();
  virtual int64_t GetGrpcResponseMessageCounts();

  virtual void SetAuthToken(const std::string &auth_token);
  virtual utils::Status AddHeaderToBackend(const std::string &key,
                                           const std::string &value);
  virtual bool FindQuery(const std::string &name, std::string *query);
  virtual bool FindHeader(const std::string &name, std::string *header);

 private:
  ngx_http_request_t *r_;
};

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // NGINX_NGX_ESP_REQUEST_H_
