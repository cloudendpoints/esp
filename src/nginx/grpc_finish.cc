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

#include "src/nginx/grpc_finish.h"

#include <map>
#include <string>

extern "C" {
#include "third_party/nginx/src/http/ngx_http.h"
#include "third_party/nginx/src/http/v2/ngx_http_v2_module.h"
}

#include "src/nginx/module.h"
#include "src/nginx/util.h"

namespace google {
namespace api_manager {
namespace nginx {

namespace {
const ngx_str_t kGrpcStatusHeaderKey = ngx_string("grpc-status");
const ngx_str_t kGrpcMessageHeaderKey = ngx_string("grpc-message");

ngx_int_t AddTrailer(ngx_http_request_t *r, const ngx_str_t &key,
                     const ngx_str_t &value) {
  ngx_table_elt_t *h = reinterpret_cast<ngx_table_elt_t *>(
      ngx_list_push(&r->headers_out.trailers));
  if (h == nullptr) {
    return NGX_ERROR;
  }
  h->key = key;
  h->value = value;
  h->hash = ngx_hash_key_lc(h->key.data, h->key.len);
  return NGX_OK;
}

ngx_int_t SerializeStatusCode(ngx_pool_t *p, const utils::Status &s,
                              ngx_str_t *out) {
  out->data = reinterpret_cast<u_char *>(ngx_pcalloc(p, NGX_INT_T_LEN));
  if (out->data == nullptr) {
    return NGX_ERROR;
  }
  out->len = ngx_sprintf(out->data, "%i", s.CanonicalCode()) - out->data;
  return NGX_OK;
}

}  // namespace

ngx_int_t GrpcFinish(
    ngx_http_request_t *r, const utils::Status &status,
    std::multimap<std::string, std::string> response_trailers) {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r);
  if (ctx != nullptr) {
    ctx->status = status;
  }

  std::string status_str = status.ToString();
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "GrpcFinish: %s",
                 status_str.c_str());

  // Per http://www.grpc.io/docs/guides/wire.html#responses at the end of the
  // gRPC response, we need to send the following trailers:
  // Trailers -> Status [Status-Message] *Custom-Metadata
  // Status -> “grpc-status”
  // Status-Message -> “grpc-message”

  // Status
  ngx_str_t code;
  if (SerializeStatusCode(r->pool, status, &code) != NGX_OK) {
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                  "Failed to serialize the gRPC status code.");
    // Returning NGX_DONE in error cases to make sure that the request is being
    // cleaned-up. If we are unable to send the trailer, we don't have a way to
    // notify the client about the error. So we just log the error and return
    // NGX_DONE to make sure that the request is closed.
    return NGX_DONE;
  }
  if (AddTrailer(r, kGrpcStatusHeaderKey, code) != NGX_OK) {
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                  "Failed to add the grpc-status trailer.");
    return NGX_DONE;
  }

  // Status-Message
  if (!status.ok()) {
    ngx_str_t message;
    if (ngx_str_copy_from_std(r->pool, status.message(), &message) != NGX_OK) {
      ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                    "Failed to convert gRPC status message.");
      return NGX_DONE;
    };
    if (AddTrailer(r, kGrpcMessageHeaderKey, message) != NGX_OK) {
      ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                    "Failed to add the grpc-message trailer.");
      return NGX_DONE;
    }
  }

  // *Custom-Metadata
  for (const auto &md : response_trailers) {
    ngx_str_t key, value;
    if (ngx_str_copy_from_std(r->pool, md.first, &key) != NGX_OK ||
        ngx_str_copy_from_std(r->pool, md.second, &value) != NGX_OK) {
      ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                    "Failed to convert gRPC custom metadata.");
      return NGX_DONE;
    }

    if (AddTrailer(r, key, value) != NGX_OK) {
      ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                    "Failed to add a gRPC custom metadata trailer.");
      return NGX_DONE;
    }
  }

  ngx_int_t rc = ngx_http_send_special(r, NGX_HTTP_LAST);
  if (rc == NGX_ERROR) {
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                  "Failed to send the trailers - rc=%d", rc);
    return NGX_DONE;
  }

  return rc;
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
