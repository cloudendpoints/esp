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

#define RETURN_IF_NULL(r, condition, ret, message)                  \
  do {                                                              \
    if ((condition) == nullptr) {                                   \
      ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, message); \
      return ret;                                                   \
    }                                                               \
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
    end = c;
  }
  return end;
}

ngx_chain_t *EncodesGrpcStatusCode(ngx_http_request_t *r, const Code &code,
                                   size_t *length) {
  ngx_chain_t *ngx_chain_status = ngx_alloc_chain_link(r->pool);
  size_t size = snprintf(nullptr, 0, kGrpcStatus, code);
  uint8_t *buffer = static_cast<uint8_t *>(ngx_palloc(r->pool, size + 1));
  if (buffer == nullptr) {
    return nullptr;
  }
  snprintf(reinterpret_cast<char *>(buffer), size + 1, kGrpcStatus, code);
  ngx_buf_t *output = static_cast<ngx_buf_t *>(ngx_calloc_buf(r->pool));
  if (output == nullptr) {
    return nullptr;
  }
  output->start = buffer;
  output->pos = output->start;
  output->end = buffer + size;
  output->last = output->end;
  output->temporary = 1;
  (*length) += size;
  ngx_chain_status->buf = output;
  ngx_chain_status->next = nullptr;
  return ngx_chain_status;
}

ngx_chain_t *EncodesGrpcMessage(ngx_http_request_t *r,
                                const std::string &message, size_t *length) {
  ngx_chain_t *ngx_chain_message = ngx_alloc_chain_link(r->pool);
  size_t size = snprintf(nullptr, 0, kGrpcMessage, message.c_str());
  uint8_t *buffer = static_cast<uint8_t *>(ngx_palloc(r->pool, size + 1));
  if (buffer == nullptr) {
    return nullptr;
  }
  snprintf(reinterpret_cast<char *>(buffer), size + 1, kGrpcMessage,
           message.c_str());
  ngx_buf_t *output = static_cast<ngx_buf_t *>(ngx_calloc_buf(r->pool));
  if (output == nullptr) {
    return nullptr;
  }
  output->start = buffer;
  output->pos = output->start;
  output->end = buffer + size;
  output->last = output->end;
  output->temporary = 1;
  (*length) += size;
  ngx_chain_message->buf = output;
  ngx_chain_message->next = nullptr;
  return ngx_chain_message;
}

ngx_chain_t *EncodesGrpcCustomTrailers(
    ngx_http_request_t *r, std::multimap<std::string, std::string> &trailers,
    ngx_chain_t *output, size_t *length) {
  if (output == nullptr) {
    return nullptr;
  }
  if (trailers.empty()) {
    return nullptr;
  }
  ngx_chain_t *last = output;
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
    last = AppendToEnd(r, last, output_buffer);
    if (last == nullptr) {
      return nullptr;
    }
    (*length) += size;
  }
  return last;
}

ngx_chain_t *EncodesGrpcTrailersFrame(ngx_http_request_t *r,
                                      ngx_chain_t *status, ngx_chain_t *message,
                                      ngx_chain_t *custom_trailers,
                                      ngx_chain_t *custom_trailers_last,
                                      size_t total) {
  ngx_chain_t *ngx_chain_trailers_frame = ngx_alloc_chain_link(r->pool);
  if (ngx_chain_trailers_frame == nullptr) {
    return nullptr;
  }
  ngx_buf_t *ngx_buf_trailers_frame =
      static_cast<ngx_buf_t *>(ngx_calloc_buf(r->pool));
  if (ngx_buf_trailers_frame == nullptr) {
    return nullptr;
  }

  ngx_buf_trailers_frame->start =
      static_cast<uint8_t *>(ngx_palloc(r->pool, 5));
  if (ngx_buf_trailers_frame->start == nullptr) {
    return nullptr;
  }
  NewFrame(GRPC_WEB_FH_TRAILER, total, ngx_buf_trailers_frame->start);
  ngx_buf_trailers_frame->end = ngx_buf_trailers_frame->start + 5;
  ngx_buf_trailers_frame->pos = ngx_buf_trailers_frame->start;
  ngx_buf_trailers_frame->last = ngx_buf_trailers_frame->end;
  ngx_buf_trailers_frame->temporary = 1;

  ngx_chain_trailers_frame->buf = ngx_buf_trailers_frame;
  ngx_chain_t *last = ngx_chain_trailers_frame;
  last->next = status;
  last = last->next;
  if (message != nullptr) {
    last->next = message;
    last = last->next;
  }
  if (custom_trailers != nullptr) {
    last->next = custom_trailers;
    last = custom_trailers_last;
  }
  if (last->buf != nullptr) {
    last->buf->last_buf = true;
    last->buf->flush = true;
  }
  return ngx_chain_trailers_frame;
}
}  // namespace

ngx_int_t GrpcWebFinish(
    ngx_http_request_t *r, const utils::Status &status,
    std::multimap<std::string, std::string> response_trailers) {
  uint64_t length = 0;

  // Encodes GRPC status.
  ngx_chain_t *grpc_status =
      EncodesGrpcStatusCode(r, status.CanonicalCode(), &length);
  RETURN_IF_NULL(r, grpc_status, NGX_DONE, "Failed to encode gRPC-Web status.");

  // Encodes GRPC message.
  ngx_chain_t *grpc_message = nullptr;
  if (!status.message().empty()) {
    grpc_message = EncodesGrpcMessage(r, status.message(), &length);
    RETURN_IF_NULL(r, grpc_message, NGX_DONE,
                   "Failed to encode gRPC-Web message.");
  }

  // Encodes GRPC trailers.
  ngx_chain_t *trailers = nullptr;
  ngx_chain_t *trailers_last = nullptr;
  if (!response_trailers.empty()) {
    trailers = ngx_alloc_chain_link(r->pool);
    RETURN_IF_NULL(
        r, trailers, NGX_DONE,
        "Failed to allocate ngx_chain_t for gRPC-Web custom trailers.");
    trailers->buf = nullptr;
    trailers->next = nullptr;
    trailers_last =
        EncodesGrpcCustomTrailers(r, response_trailers, trailers, &length);
    RETURN_IF_NULL(r, trailers_last, NGX_DONE,
                   "Failed to encode gRPC-Web custom trailers.");
  }

  // Encodes GRPC trailer frame.
  ngx_chain_t *output = EncodesGrpcTrailersFrame(
      r, grpc_status, grpc_message, trailers, trailers_last, length);
  RETURN_IF_NULL(r, output, NGX_DONE,
                 "Failed to encode gRPC-Web trailers frame.");

  ngx_int_t rc = ngx_http_output_filter(r, output);
  if (rc == NGX_ERROR) {
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                  "Failed to send the gRPC-Web trailers frame - rc=%d", rc);
    return NGX_DONE;
  }
  return rc;
}
}  // namespace nginx
}  // namespace api_manager
}  // namespace google
