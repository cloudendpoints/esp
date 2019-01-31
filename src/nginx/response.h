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
#ifndef NGINX_NGX_ESP_RESPONSE_H_
#define NGINX_NGX_ESP_RESPONSE_H_

extern "C" {
#include "src/http/ngx_http.h"
}

#include "include/api_manager/response.h"

namespace google {
namespace api_manager {
namespace nginx {

// Wraps ngx_http_request_t as a ::google::api_manager::Response.
class NgxEspResponse : public Response {
 public:
  NgxEspResponse(ngx_http_request_t *r);
  virtual ~NgxEspResponse();

  virtual utils::Status GetResponseStatus();
  virtual std::size_t GetRequestSize();
  virtual std::size_t GetResponseSize();
  virtual utils::Status GetLatencyInfo(service_control::LatencyInfo *info);
  virtual bool FindHeader(const std::string &name, std::string *header) const;

 private:
  ngx_http_request_t *r_;
};

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // NGINX_NGX_ESP_RESPONSE_H_
