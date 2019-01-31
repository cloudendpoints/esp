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
#include "src/nginx/response.h"

#include "src/nginx/module.h"
#include "src/nginx/util.h"

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {
namespace nginx {

NgxEspResponse::NgxEspResponse(ngx_http_request_t *r) : r_(r) {}

NgxEspResponse::~NgxEspResponse() {}

Status NgxEspResponse::GetResponseStatus() {
  // This is tracky with GRPC. Grpc sends status in trailers
  // which ngx_http_get_response_status() will not return.
  // For HTTP, we have to call ngx_http_get_response_status()
  // to get the upstream response status since we use Nginx proxy_pass
  // directly.
  // If check access handler fails, its status is in ctx->status
  // Grpc backend status is also in ctx->status. Only the
  // HTTP upstream status should be got from ngx_http_get_response_status
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r_);
  if (ctx && !ctx->status.ok()) {
    return ctx->status;
  }

  auto error_cause =
      ctx == nullptr ? Status::INTERNAL : ctx->status.error_cause();
  return Status(ngx_http_get_response_status(r_), "", error_cause);
}

std::size_t NgxEspResponse::GetRequestSize() { return r_->request_length; }

std::size_t NgxEspResponse::GetResponseSize() { return r_->connection->sent; }

Status NgxEspResponse::GetLatencyInfo(service_control::LatencyInfo *info) {
  ngx_time_t *tp = ngx_timeofday();
  info->request_time_ms =
      ((tp->sec - r_->start_sec) * 1000 + (tp->msec - r_->start_msec));
  if (r_->upstream_states != nullptr) {
    info->backend_time_ms = 0;
    ngx_http_upstream_state_t *states =
        reinterpret_cast<ngx_http_upstream_state_t *>(
            r_->upstream_states->elts);
    for (ngx_uint_t i = 0; i < r_->upstream_states->nelts; ++i) {
      info->backend_time_ms += states[i].response_time;
    }
  } else {
    ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r_);
    if (ctx && ctx->backend_time >= 0) {
      info->backend_time_ms = ctx->backend_time;
    }
  }
  if (info->backend_time_ms >= 0) {
    info->overhead_time_ms =
        ngx_max(0, info->request_time_ms - info->backend_time_ms);
  } else {
    // The backend time is unavailable, which can happen, for example,
    // the request failed at an earlier step (e.g., auth check) and
    // did not reach backend.
    info->overhead_time_ms = info->request_time_ms;
  }
  ngx_log_debug3(
      NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
      "ngx response: request time=%d, backend time=%d, overhead time=%d",
      info->request_time_ms, info->backend_time_ms, info->overhead_time_ms);
  return Status::OK;
}

bool NgxEspResponse::FindHeader(const std::string &name,
                                std::string *header) const {
  auto h = ngx_esp_find_headers_out(
      r_, reinterpret_cast<u_char *>(const_cast<char *>(name.data())),
      name.size());
  if (h && h->value.len > 0) {
    *header = ngx_str_to_std(h->value);
    return true;
  }
  return false;
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
