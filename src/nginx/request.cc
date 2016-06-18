// Copyright (C) Endpoints Server Proxy Authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "src/nginx/request.h"

#include "include/api_manager/auth.h"
#include "src/api_manager/auth/lib/auth_token.h"
#include "src/api_manager/auth/lib/json.h"
#include "src/nginx/module.h"
#include "src/nginx/util.h"

namespace google {
namespace api_manager {
namespace nginx {

NgxEspRequest::NgxEspRequest(ngx_http_request_t *r) : r_(r) {}

NgxEspRequest::~NgxEspRequest() {
  // TODO: Propagate any changes to the headers back to the request.
}

std::string NgxEspRequest::GetRequestHTTPMethod() {
  return ngx_str_to_std(r_->method_name);
}

std::string NgxEspRequest::GetRequestPath() { return ngx_str_to_std(r_->uri); }

std::string NgxEspRequest::GetUnparsedRequestPath() {
  return ngx_str_to_std(r_->unparsed_uri);
}

::google::api_manager::protocol::Protocol NgxEspRequest::GetRequestProtocol() {
  if (r_ && r_->connection) {
#if (NGX_SSL)
    if (r_->connection->ssl) {
      return ::google::api_manager::protocol::HTTPS;
    }
#endif
    return ::google::api_manager::protocol::HTTP;
  } else {
    return ::google::api_manager::protocol::UNKNOWN;
  }
}

std::string NgxEspRequest::GetClientIP() {
  // use remote_addr varaible to get client_ip.
  ngx_esp_main_conf_t *mc = reinterpret_cast<ngx_esp_main_conf_t *>(
      ngx_http_get_module_main_conf(r_, ngx_esp_module));
  if (mc->remote_addr_variable_index != NGX_ERROR) {
    ngx_http_variable_value_t *vv =
        ngx_http_get_indexed_variable(r_, mc->remote_addr_variable_index);
    if (vv != nullptr && !vv->not_found) {
      return ngx_str_to_std(ngx_str_t({vv->len, vv->data}));
    }
  }
  return "";
}

bool NgxEspRequest::FindQuery(const std::string &name, std::string *query) {
  ngx_str_t out = ngx_null_string;
  ngx_http_arg(r_, reinterpret_cast<u_char *>(const_cast<char *>(name.data())),
               name.size(), &out);
  if (out.len > 0) {
    *query = ngx_str_to_std(out);
    return true;
  }
  return false;
}

bool NgxEspRequest::FindHeader(const std::string &name, std::string *header) {
  auto h = ngx_esp_find_headers_in(
      r_, reinterpret_cast<u_char *>(const_cast<char *>(name.data())),
      name.size());
  if (h && h->value.len > 0) {
    *header = ngx_str_to_std(h->value);
    return true;
  }
  return false;
}

void NgxEspRequest::SetUserInfo(const UserInfo &user_info) {
  char *json_buf = auth::WriteUserInfoToJson(user_info);
  if (json_buf == nullptr) {
    return;
  }

  size_t json_size = strlen(json_buf);
  ngx_str_t dst = ngx_null_string;
  dst.data = reinterpret_cast<u_char *>(
      ngx_palloc(r_->pool, ngx_base64_encoded_length(json_size)));
  if (dst.data != nullptr) {
    ngx_str_t src = {json_size, reinterpret_cast<u_char *>(json_buf)};
    ngx_encode_base64(&dst, &src);
    ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r_);
    ctx->endpoints_api_userinfo = dst;
  }
  auth::esp_grpc_free(json_buf);
}

void NgxEspRequest::SetAuthToken(const std::string &auth_token) {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r_);
  ngx_str_copy_from_std(r_->pool, auth_token, &ctx->auth_token);
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
