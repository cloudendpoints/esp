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

#include "src/nginx/transcoded_grpc_server_call.h"

#include <map>
#include <string>
#include <vector>

#include "grpc++/support/byte_buffer.h"
#include "src/core/lib/slice/b64.h"
#include "src/nginx/error.h"
#include "src/nginx/grpc.h"
#include "src/nginx/module.h"
#include "src/nginx/util.h"

extern "C" {
#include "src/http/v2/ngx_http_v2_module.h"
}

namespace google {
namespace api_manager {
namespace nginx {

namespace {
const ngx_str_t kContentTypeApplicationJson = ngx_string("application/json");

const std::string kGrpcStatusDetailsBin = "grpc-status-details-bin";
}  // namespace

NgxEspTranscodedGrpcServerCall::NgxEspTranscodedGrpcServerCall(
    ngx_http_request_t *r,
    std::unique_ptr<NgxRequestZeroCopyInputStream> nginx_request_stream,
    std::unique_ptr<grpc::GrpcZeroCopyInputStream> grpc_response_stream,
    std::unique_ptr<::google::grpc::transcoding::Transcoder> transcoder)
    : NgxEspGrpcServerCall(r, true),
      nginx_request_stream_(std::move(nginx_request_stream)),
      grpc_response_stream_(std::move(grpc_response_stream)),
      transcoder_(std::move(transcoder)) {}

utils::Status NgxEspTranscodedGrpcServerCall::Create(
    ngx_http_request_t *r,
    std::shared_ptr<NgxEspTranscodedGrpcServerCall> *out) {
  // Create ZeroCopyInputStream implementations over the request and response
  std::unique_ptr<NgxRequestZeroCopyInputStream> nginx_request_stream(
      new NgxRequestZeroCopyInputStream(r));
  std::unique_ptr<grpc::GrpcZeroCopyInputStream> grpc_response_stream(
      new grpc::GrpcZeroCopyInputStream());

  // Make sure the ESP request context and the request handler exist
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r);
  if (!ctx || !ctx->request_handler) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "ESP request context or request handler is NULL.");
    return utils::Status(
        NGX_HTTP_INTERNAL_SERVER_ERROR,
        "Internal error occurred while converting response message.");
  }

  // Create the Transcoder
  std::unique_ptr<::google::grpc::transcoding::Transcoder> transcoder;
  auto protoStatus = ctx->transcoder_factory->Create(
      *ctx->request_handler->method_call(), nginx_request_stream.get(),
      grpc_response_stream.get(), &transcoder);
  if (!protoStatus.ok()) {
    return utils::Status::FromProto(protoStatus);
  }

  // Create the NgxEspTranscodedGrpcServerCall instance
  std::shared_ptr<NgxEspTranscodedGrpcServerCall> call(
      new NgxEspTranscodedGrpcServerCall(r, std::move(nginx_request_stream),
                                         std::move(grpc_response_stream),
                                         std::move(transcoder)));
  auto status = call->ProcessPrereadRequestBody();
  if (!status.ok()) {
    return status;
  }

  *out = call;
  return utils::Status::OK;
}

const ngx_str_t &NgxEspTranscodedGrpcServerCall::response_content_type() const {
  return kContentTypeApplicationJson;
}

void NgxEspTranscodedGrpcServerCall::Finish(
    const utils::Status &status,
    std::multimap<std::string, std::string> response_trailers) {
  if (!cln_.data) {
    return;
  }

  if (!status.ok()) {
    // If grpc response trailers have a "grpc-status-details-bin" header,
    // use base64 to decode that value, parse it to proto and save it in status.
    const auto &it = response_trailers.find(kGrpcStatusDetailsBin);
    if (it != response_trailers.end() && !it->second.empty()) {
      ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r_);
      if (ctx) {
        ::grpc::Slice value_slice(
            grpc_base64_decode_with_len(it->second.c_str(), it->second.length(),
                                        false),
            ::grpc::Slice::STEAL_REF);
        ctx->grpc_status_details =
            std::string(reinterpret_cast<const char *>(value_slice.begin()),
                        value_slice.size());
      }
    }
    HandleError(status);
    return;
  }

  // Make sure the headers have been sent
  if (!r_->header_sent) {
    auto status = WriteDownstreamHeaders();
    if (!status.ok()) {
      HandleError(status);
      return;
    }
  }

  // Finish the Transcoder input response stream and read the translated
  // response output.
  grpc_response_stream_->Finish();
  ngx_chain_t out;
  if (!ReadTranslatedResponse(&out)) {
    return;
  }
  // Mark this as the last buffer in the request
  out.buf->last_buf = 1;

  // Send the final buffer and finalize the request
  ngx_int_t rc = ngx_http_output_filter(r_, &out);
  if (rc == NGX_ERROR) {
    ngx_log_error(NGX_LOG_DEBUG, r_->connection->log, 0,
                  "Failed to send the last buffer - rc=%d", rc);
    // Converting NGX_ERROR to NGX_DONE to make ngx_http_finalize_request()
    // close the request.
    rc = NGX_DONE;
  }
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                 "NgxEspTranscodedGrpcServerCall::Finish: the final message "
                 "sent. Finalizing the request with status %d.",
                 rc);
  ngx_http_finalize_request(r_, rc);
}

bool NgxEspTranscodedGrpcServerCall::ConvertRequestBody(
    std::vector<grpc_slice> *out) {
  const void *buffer = nullptr;
  int size = 0;
  // Get the next translated buffer from the Transcoder & add a slice to the
  // output.
  while (transcoder_->RequestOutput()->Next(&buffer, &size) && size > 0) {
    out->push_back(grpc_slice_from_copied_buffer(
        reinterpret_cast<const char *>(buffer), size));
  }
  // Check the status
  if (!transcoder_->RequestStatus().ok()) {
    HandleError(utils::Status::FromProto(transcoder_->RequestStatus()));
    return false;
  }
  if (!nginx_request_stream_->Status().ok()) {
    HandleError(nginx_request_stream_->Status());
    return false;
  }
  return true;
}

bool NgxEspTranscodedGrpcServerCall::ConvertResponseMessage(
    const ::grpc::ByteBuffer &msg, ngx_chain_t *out) {
  // Serialize ::grpc::ByteBuffer into grpc_byte_buffer
  grpc_byte_buffer *grpc_msg = ConvertByteBuffer(msg);
  if (!grpc_msg) {
    HandleError(utils::Status(
        NGX_HTTP_INTERNAL_SERVER_ERROR,
        "Internal error occurred while converting response message."));
    return false;
  }

  // Add the response gRPC message to the Transcoder input response stream and
  // read the translated response from the transcoder.
  grpc_response_stream_->AddMessage(grpc_msg, true);
  return ReadTranslatedResponse(out);
}

bool NgxEspTranscodedGrpcServerCall::ReadTranslatedResponse(ngx_chain_t *out) {
  // Allocate an ngx_buf.
  ngx_buf_t *buf = reinterpret_cast<ngx_buf_t *>(ngx_calloc_buf(r_->pool));
  if (!buf) {
    ngx_log_error(NGX_LOG_ERR, r_->connection->log, 0,
                  "Failed to allocate response buffer header for GRPC "
                  "response message.");
    return false;
  }

  // Read the translated response into an ngx_buf.
  const void *buffer = nullptr;
  int size = 0;
  if (transcoder_->ResponseOutput()->Next(&buffer, &size) && size > 0) {
    ngx_log_debug1(
        NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
        "NgxEspTranscodedGrpcServerCall: Write => %s",
        std::string(reinterpret_cast<const char *>(buffer), size).c_str());

    buf->start = reinterpret_cast<u_char *>(const_cast<void *>(buffer));
    buf->end = buf->start + size;
    buf->pos = buf->start;
    buf->last = buf->pos + size;
    buf->temporary = 1;
  } else if (!transcoder_->ResponseStatus().ok()) {
    HandleError(utils::Status::FromProto(transcoder_->ResponseStatus()));
    return false;
  }
  // If the transcoder doesn't return any data, we will return an empty ngx_buf

  buf->last_in_chain = 1;
  buf->flush = 1;
  out->next = nullptr;
  out->buf = buf;

  return true;
}

void NgxEspTranscodedGrpcServerCall::HandleError(const utils::Status &error) {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r_);
  if (ctx) {
    ctx->status = error;
  }
  return ngx_http_finalize_request(r_, ngx_esp_return_error(r_));
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
