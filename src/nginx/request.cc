// Copyright (C) Extensible Service Proxy Authors
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

#include "src/api_manager/check_auth.h"
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

std::string NgxEspRequest::GetQueryParameters() {
  return ngx_str_to_std(r_->args);
}

std::string NgxEspRequest::GetRequestPath() {
  std::string unparsed_str = ngx_str_to_std(r_->unparsed_uri);
  return unparsed_str.substr(0, unparsed_str.find_first_of('?'));
}

std::string NgxEspRequest::GetUnparsedRequestPath() {
  return ngx_str_to_std(r_->unparsed_uri);
}

::google::api_manager::protocol::Protocol NgxEspRequest::GetFrontendProtocol() {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_get_module_ctx(r_);
  if (ctx->grpc_pass_through) {
    return ::google::api_manager::protocol::GRPC;
  }
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

::google::api_manager::protocol::Protocol NgxEspRequest::GetBackendProtocol() {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_get_module_ctx(r_);
  if (ctx->grpc_backend) {
    return ::google::api_manager::protocol::GRPC;
  } else {
    // TODO: determine HTTP or HTTPS for backend.
    return ::google::api_manager::protocol::UNKNOWN;
  }
}

std::string NgxEspRequest::GetClientIP() {
  // use remote_addr variable to get client_ip.
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

int64_t NgxEspRequest::GetGrpcRequestMessageCounts() {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_get_module_ctx(r_);
  return ctx->grpc_request_message_counts;
}

int64_t NgxEspRequest::GetGrpcResponseMessageCounts() {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_get_module_ctx(r_);
  return ctx->grpc_response_message_counts;
}

int64_t NgxEspRequest::GetGrpcRequestBytes() {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_get_module_ctx(r_);
  return ctx->grpc_request_bytes;
}

int64_t NgxEspRequest::GetGrpcResponseBytes() {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_get_module_ctx(r_);
  return ctx->grpc_response_bytes;
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

void NgxEspRequest::SetAuthToken(const std::string &auth_token) {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_get_module_ctx(r_);
  ngx_str_copy_from_std(r_->pool, auth_token, &ctx->auth_token);
}

utils::Status NgxEspRequest::AddHeaderToBackend(const std::string &key,
                                                const std::string &value) {
  ngx_table_elt_t *h = nullptr;
  for (auto &h_in : r_->headers_in) {
    if (key.size() == h_in.key.len &&
        strncasecmp(key.c_str(), reinterpret_cast<const char *>(h_in.key.data),
                    h_in.key.len) == 0) {
      h = &h_in;
      break;
    }
  }
  if (h == nullptr) {
    h = reinterpret_cast<ngx_table_elt_t *>(
        ngx_list_push(&r_->headers_in.headers));
    if (h == nullptr) {
      return utils::Status(Code::INTERNAL, "Out of memory");
    }

    h->lowcase_key =
        reinterpret_cast<u_char *>(ngx_pcalloc(r_->pool, key.size()));
    if (h->lowcase_key == nullptr) {
      return utils::Status(Code::INTERNAL, "Out of memory");
    }
    h->hash = ngx_hash_strlow(
        h->lowcase_key,
        reinterpret_cast<u_char *>(const_cast<char *>(key.c_str())),
        key.size());
  }

  if (ngx_str_copy_from_std(r_->pool, key, &h->key) != NGX_OK ||
      ngx_str_copy_from_std(r_->pool, value, &h->value) != NGX_OK) {
    return utils::Status(Code::INTERNAL, "Out of memory");
  }
  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                 "updates header to backend: \"%V: %V\"", &h->key, &h->value);
  return utils::Status::OK;
}

}  // namespace nginx
}  // namespace api_manager

}  // namespace google
