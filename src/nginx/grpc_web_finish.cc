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

#include "src/nginx/grpc_finish.h"

extern "C" {
#include "src/http/ngx_http.h"
}

#define FAILED_TO_SEND_GRPC_WEB_TRAILERS \
  "Failed to serialize the gRPC-Web trailers."

#define RETURN_IF_NULL(r, condition, ret)                 \
  do {                                                    \
    if (condition == nullptr) {                           \
      ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, \
                    FAILED_TO_SEND_GRPC_WEB_TRAILERS);    \
      return ret;                                         \
    }                                                     \
  } while (0)

namespace google {
namespace api_manager {
namespace nginx {

namespace {
const char kGrpcStatus[] = "grpc-status: %i\r\n";
const char kGrpcMessage[] = "grpc-message: %s\r\n";

// GRPC Web message frame.
const uint8_t GRPC_WEB_FH_DATA = 0b0u;
// GRPC Web trailer frame.
const uint8_t GRPC_WEB_FH_TRAILER = 0b10000000u;

// Creates a new GRPC data frame with the given flags and length.
// @param flags supplies the GRPC data frame flags.
// @param length supplies the GRPC data frame length.
// @param output the buffer to store the encoded data, it's size must be 5.
void NewFrame(uint8_t flags, uint32_t length, uint8_t *output) {
  output[0] = flags;
  output[1] = static_cast<uint8_t>(length >> 24);
  output[2] = static_cast<uint8_t>(length >> 16);
  output[3] = static_cast<uint8_t>(length >> 8);
  output[4] = static_cast<uint8_t>(length);
}

// Appends the buffer to the end of the given chain. Returns the end of the
// chain or nullptr if any error happens.
ngx_chain_t *AppendToEnd(ngx_http_request_t *r, ngx_chain_t *chain,
                         ngx_buf_t *buffer) {
  if (buffer == nullptr) {
    return chain;
  }

  ngx_chain_t *end = chain;
  while (end->next != nullptr) {
    end = end->next;
  }
  if (end->buf == nullptr) {
    end->buf = buffer;
  } else {
    ngx_chain_t *c = ngx_alloc_chain_link(r->pool);
    if (c == nullptr) {
      return nullptr;
    }
    c->buf = buffer;
    c->next = nullptr;
    end->next = c;
  }
  return end;
}

ngx_buf_t *EncodesGrpcStatusCode(ngx_http_request_t *r, const Code &code) {
  size_t size = snprintf(nullptr, 0, kGrpcStatus, code);
  uint8_t *status = static_cast<uint8_t *>(ngx_palloc(r->pool, size + 1));
  if (status == nullptr) {
    return nullptr;
  }
  snprintf(reinterpret_cast<char *>(status), size + 1, kGrpcStatus, code);
  ngx_buf_t *output = static_cast<ngx_buf_t *>(ngx_calloc_buf(r->pool));
  if (output == nullptr) {
    return nullptr;
  }
  output->start = status;
  output->pos = output->start;
  output->end = status + size;
  output->last = output->end;
  output->temporary = 1;
  return output;
}

ngx_buf_t *EncodesGrpcMessage(ngx_http_request_t *r,
                              const std::string &message) {
  size_t size = snprintf(nullptr, 0, kGrpcMessage, message.c_str());
  uint8_t *status = static_cast<uint8_t *>(ngx_palloc(r->pool, size + 1));
  if (status == nullptr) {
    return nullptr;
  }
  snprintf(reinterpret_cast<char *>(status), size + 1, kGrpcMessage,
           message.c_str());
  ngx_buf_t *output = static_cast<ngx_buf_t *>(ngx_calloc_buf(r->pool));
  if (output == nullptr) {
    return nullptr;
  }
  output->start = status;
  output->pos = output->start;
  output->end = status + size;
  output->last = output->end;
  output->temporary = 1;
  return output;
}

ngx_chain_t *EncodesGrpcTrailers(
    ngx_http_request_t *r, std::multimap<std::string, std::string> &trailers,
    size_t *length) {
  if (trailers.empty()) {
    return nullptr;
  }

  ngx_chain_t *output = ngx_alloc_chain_link(r->pool);
  if (output == nullptr) {
    return nullptr;
  }
  ngx_chain_t *current = output;

  for (auto &trailer : trailers) {
    size_t size = trailer.first.size() + trailer.second.size() + 4;
    uint8_t *grpc_trailer = static_cast<uint8_t *>(ngx_palloc(r->pool, size));
    uint8_t *p = grpc_trailer;
    memcpy(p, trailer.first.c_str(), trailer.first.size());
    p += trailer.first.size();
    memcpy(p, ": ", 2);
    p += 2;
    memcpy(p, trailer.second.data(), trailer.second.size());
    p += trailer.second.size();
    memcpy(p, "\r\n", 2);
    ngx_buf_t *output_buffer =
        static_cast<ngx_buf_t *>(ngx_calloc_buf(r->pool));
    if (output_buffer == nullptr) {
      return nullptr;
    }
    output_buffer->start = grpc_trailer;
    output_buffer->pos = output_buffer->start;
    output_buffer->end = grpc_trailer + size;
    output_buffer->last = output_buffer->end;
    output_buffer->temporary = 1;
    current = AppendToEnd(r, current, output_buffer);
    if (current == nullptr) {
      return nullptr;
    }

    *length += size;
  }
  return output;
}
}  // namespace

ngx_int_t GrpcWebFinish(
    ngx_http_request_t *r, const utils::Status &status,
    std::multimap<std::string, std::string> response_trailers) {
  uint64_t length = 0;

  // Encodes GRPC status.
  ngx_buf_t *grpc_status = EncodesGrpcStatusCode(r, status.CanonicalCode());
  RETURN_IF_NULL(r, grpc_status, NGX_DONE);
  length += grpc_status->end - grpc_status->start;

  // Encodes GRPC message.
  ngx_buf_t *grpc_message = nullptr;
  if (!status.message().empty()) {
    grpc_message = EncodesGrpcMessage(r, status.message());
    RETURN_IF_NULL(r, grpc_message, NGX_DONE);
    length += grpc_message->end - grpc_message->start;
  }

  // Encodes GRPC trailers.
  ngx_chain_t *trailers = EncodesGrpcTrailers(r, response_trailers, &length);
  RETURN_IF_NULL(r, trailers, NGX_DONE);

  // Encodes GRPC trailer frame.
  ngx_buf_t *frame = static_cast<ngx_buf_t *>(ngx_calloc_buf(r->pool));
  RETURN_IF_NULL(r, frame, NGX_DONE);
  frame->start = static_cast<uint8_t *>(ngx_palloc(r->pool, 5));
  RETURN_IF_NULL(r, frame->start, NGX_DONE);
  NewFrame(GRPC_WEB_FH_TRAILER, length, frame->start);
  frame->end = frame->start + 5;
  frame->pos = frame->start;
  frame->last = frame->end;
  frame->temporary = 1;

  ngx_chain_t *output = AppendToEnd(r, r->out, frame);
  RETURN_IF_NULL(r, output, NGX_DONE);
  output = AppendToEnd(r, output, grpc_status);
  RETURN_IF_NULL(r, output, NGX_DONE);
  output = AppendToEnd(r, output, grpc_message);
  RETURN_IF_NULL(r, output, NGX_DONE);
  output->next = trailers;

  ngx_int_t rc = ngx_http_send_special(r, NGX_HTTP_LAST);
  if (rc == NGX_ERROR) {
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                  "Failed to send the gRPC-Web trailers - rc=%d", rc);
    return NGX_DONE;
  }
  return rc;
}
}  // namespace nginx
}  // namespace api_manager
}  // namespace google
