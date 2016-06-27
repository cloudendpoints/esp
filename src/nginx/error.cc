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
//
// An Endpoints Server Proxy error handling.
//

#include "src/nginx/error.h"
#include "src/nginx/grpc_finish.h"
#include "src/nginx/module.h"
#include "src/nginx/util.h"

namespace google {
namespace api_manager {
namespace nginx {
namespace {

ngx_str_t application_json = ngx_string("application/json");
ngx_str_t application_grpc = ngx_string("application/grpc");

ngx_str_t www_authenticate = ngx_string("WWW-Authenticate");
const u_char www_authenticate_lowcase[] = "www-authenticate";
ngx_str_t missing_credential = ngx_string("Bearer");
ngx_str_t invalid_token = ngx_string("Bearer, error=\"invalid_token\"");

}  // namespace

ngx_int_t ngx_esp_return_json_error(ngx_http_request_t *r,
                                    utils::Status status) {
  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "Return JSON error: Status code: %d message: %s",
                 status.code(), status.message().c_str());

  if (status.code() == NGX_HTTP_CLOSE) {
    return status.code();
  }

  r->err_status = status.HttpCode();

  if (ngx_http_discard_request_body(r) != NGX_OK) {
    r->keepalive = 0;
  }

  if (status.message().empty()) {
    status.SetMessage("Unknown Error");
  }

  ngx_str_t json_error;
  if (ngx_str_copy_from_std(r->pool, status.ToJson(), &json_error) != NGX_OK) {
    return NGX_ERROR;
  }

  r->headers_out.content_type = application_json;
  r->headers_out.content_type_len = application_json.len;
  r->headers_out.content_type_lowcase = nullptr;
  r->headers_out.content_length_n = json_error.len;

  // Clear Content-Length header; it will be added by a header filter later.
  if (r->headers_out.content_length) {
    r->headers_out.content_length->hash = 0;
    r->headers_out.content_length = nullptr;
  }

  // Returns WWW-Authenticate header for 401 response.
  // See https://tools.ietf.org/html/rfc6750#section-3.
  if (status.HttpCode() == NGX_HTTP_UNAUTHORIZED) {
    r->headers_out.www_authenticate = reinterpret_cast<ngx_table_elt_t *>(
        ngx_list_push(&r->headers_out.headers));
    if (r->headers_out.www_authenticate == nullptr) {
      return NGX_ERROR;
    }

    r->headers_out.www_authenticate->key = www_authenticate;
    r->headers_out.www_authenticate->lowcase_key =
        const_cast<u_char *>(www_authenticate_lowcase);
    r->headers_out.www_authenticate->hash =
        ngx_hash_key(const_cast<u_char *>(www_authenticate_lowcase),
                     sizeof(www_authenticate_lowcase) - 1);

    ngx_esp_request_ctx_t *request_ctx = ngx_http_esp_ensure_module_ctx(r);
    if (request_ctx->auth_token.len == 0) {
      r->headers_out.www_authenticate->value = missing_credential;
    } else {
      r->headers_out.www_authenticate->value = invalid_token;
    }
  }

  ngx_http_clear_accept_ranges(r);
  ngx_http_clear_last_modified(r);
  ngx_http_clear_etag(r);

  ngx_int_t rc = ngx_http_send_header(r);
  if (rc == NGX_ERROR || r->header_only) {
    return NGX_DONE;
  }

  ngx_buf_t *body = reinterpret_cast<ngx_buf_t *>(ngx_calloc_buf(r->pool));
  if (body == nullptr) {
    return NGX_ERROR;
  }

  body->temporary = 1;
  body->pos = json_error.data;
  body->last = json_error.data + json_error.len;
  body->last_in_chain = 1;

  if (r == r->main) {
    body->last_buf = 1;
  }

  ngx_chain_t out = {body, nullptr};
  return ngx_http_output_filter(r, &out);
}

ngx_int_t ngx_esp_return_grpc_error(ngx_http_request_t *r,
                                    utils::Status status) {
  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "Return GRPC error: Status code: %d message: %s",
                 status.code(), status.message().c_str());

  if (status.code() == NGX_HTTP_CLOSE) {
    return status.code();
  }

  if (ngx_http_discard_request_body(r) != NGX_OK) {
    r->keepalive = 0;
  }

  // GRPC always use 200 OK as HTTP status
  r->headers_out.status = NGX_HTTP_OK;
  r->headers_out.content_type = application_grpc;
  r->headers_out.content_type_len = application_grpc.len;
  r->headers_out.content_type_lowcase = nullptr;

  ngx_http_clear_content_length(r);
  ngx_http_clear_accept_ranges(r);
  ngx_http_clear_last_modified(r);
  ngx_http_clear_etag(r);

  ngx_int_t rc = ngx_http_send_header(r);
  if (rc == NGX_ERROR) {
    return NGX_DONE;
  }

  ngx_http_output_filter(r, nullptr);

  return GrpcFinish(r, status, {});
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
