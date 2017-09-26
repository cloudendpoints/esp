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

#include "src/nginx/grpc_passthrough_server_call.h"

#include <map>
#include <string>
#include <vector>

#include "grpc++/support/byte_buffer.h"
#include "src/nginx/error.h"
#include "src/nginx/grpc_finish.h"
#include "src/nginx/util.h"

extern "C" {
#include "src/http/v2/ngx_http_v2_module.h"
}

namespace google {
namespace api_manager {
namespace nginx {

namespace {
const ngx_str_t kContentTypeApplicationGrpc = ngx_string("application/grpc");

// Deletes GRPC objects.
//
// This is used (instead of specializing std::default_deleter<>) in
// order to explicitly verify that the deleter's in scope.  Otherwise,
// it'd be entirely too easy for sources to include grpc.h without
// pulling in the corresponding deleter definitions.
struct GrpcDeleter {
  void operator()(grpc_byte_buffer *byte_buffer) {
    grpc_byte_buffer_destroy(byte_buffer);
  }
};
}  // namespace

NgxEspGrpcPassThroughServerCall::NgxEspGrpcPassThroughServerCall(
    ngx_http_request_t *r)
    : NgxEspGrpcServerCall(r, false) {}

utils::Status NgxEspGrpcPassThroughServerCall::Create(
    ngx_http_request_t *r,
    std::shared_ptr<NgxEspGrpcPassThroughServerCall> *out) {
  std::shared_ptr<NgxEspGrpcPassThroughServerCall> call(
      new NgxEspGrpcPassThroughServerCall(r));
  auto status = call->ProcessPrereadRequestBody();

  if (!status.ok()) {
    return status;
  }

  *out = call;
  return utils::Status::OK;
}

const ngx_str_t &NgxEspGrpcPassThroughServerCall::response_content_type()
    const {
  return kContentTypeApplicationGrpc;
}

void NgxEspGrpcPassThroughServerCall::Finish(
    const utils::Status &status,
    std::multimap<std::string, std::string> response_trailers) {
  if (!cln_.data) {
    return;
  }

  // Make sure the headers have been sent
  if (!r_->header_sent) {
    auto status = WriteDownstreamHeaders();
    if (!status.ok()) {
      ngx_http_finalize_request(r_, GrpcFinish(r_, status, response_trailers));
      return;
    }
  }

  ngx_http_finalize_request(r_, GrpcFinish(r_, status, response_trailers));
}

bool NgxEspGrpcPassThroughServerCall::ConvertRequestBody(
    std::vector<grpc_slice> *out) {
  // Turn all incoming buffers into slices.
  ngx_http_request_body_t *body = r_->request_body;
  while (body->bufs) {
    ngx_chain_t *cl = body->bufs;
    body->bufs = cl->next;
    out->push_back(GrpcSliceFromNginxBuffer(cl->buf));
    cl->next = body->free;
    body->free = cl;
  }
  return true;
}

bool NgxEspGrpcPassThroughServerCall::ConvertResponseMessage(
    const ::grpc::ByteBuffer &msg, ngx_chain_t *out) {
  grpc_byte_buffer *grpc_msg = nullptr;
  bool own_buffer;

  if (!::grpc::SerializationTraits<::grpc::ByteBuffer>::Serialize(
           msg, &grpc_msg, &own_buffer)
           .ok() ||
      !grpc_msg) {
    return false;
  }

  auto msg_deleter = std::unique_ptr<grpc_byte_buffer, GrpcDeleter>();
  if (own_buffer) {
    msg_deleter.reset(grpc_msg);
  }

  // Since there's no good way to reuse the underlying grpc_slice for
  // the nginx buffer, we need to allocate an nginx buffer and copy
  // the data into it.
  size_t buflen = 5;  // Compressed flag + four bytes of length.

  // Get the length of the actual message.  N.B. This is the
  // *compressed* length.
  size_t msglen = grpc_byte_buffer_length(grpc_msg);
  buflen += msglen;

  // Allocate the chain link and buffer.
  ngx_buf_t *buf = ngx_create_temp_buf(r_->pool, buflen);
  if (!buf) {
    ngx_log_error(
        NGX_LOG_ERR, r_->connection->log, 0,
        "Failed to allocate response buffer header for GRPC response message.");
    return false;
  }
  buf->last_in_chain = 1;
  buf->flush = 1;
  out->next = nullptr;
  out->buf = buf;

  // Write the 'compressed' flag.
  *buf->last++ = (grpc_msg->data.raw.compression == GRPC_COMPRESS_NONE ? 0 : 1);

  // Write the message length: four bytes, big-endian.
  // TODO: We should fail if asked to forward a message with length > uint32_max
  buf->last[3] = msglen & 0xFF;
  msglen >>= 8;
  buf->last[2] = msglen & 0xFF;
  msglen >>= 8;
  buf->last[1] = msglen & 0xFF;
  msglen >>= 8;
  buf->last[0] = msglen & 0xFF;
  buf->last += 4;

  // Fill in the message.
  for (size_t sln = 0; sln < grpc_msg->data.raw.slice_buffer.count; sln++) {
    grpc_slice *slice = grpc_msg->data.raw.slice_buffer.slices + sln;
    ngx_memcpy(buf->last, GRPC_SLICE_START_PTR(*slice),
               GRPC_SLICE_LENGTH(*slice));
    buf->last += GRPC_SLICE_LENGTH(*slice);
  }

  return true;
}

grpc_slice NgxEspGrpcPassThroughServerCall::GrpcSliceFromNginxBuffer(
    ngx_buf_t *buf) {
  if (!ngx_buf_in_memory(buf) && buf->file) {
    // If the buffer's not in memory, we need to read the contents.
    grpc_slice result = grpc_slice_malloc(ngx_buf_size(buf));
    ngx_read_file(buf->file, GRPC_SLICE_START_PTR(result), ngx_buf_size(buf),
                  buf->file_pos);
    return result;
  }

  // Otherwise, just copy the buffer's data.
  grpc_slice result = grpc_slice_from_copied_buffer(
      reinterpret_cast<char *>(buf->pos), buf->last - buf->pos);

  buf->pos += GRPC_SLICE_LENGTH(result);
  return result;
}
}  // namespace nginx
}  // namespace api_manager
}  // namespace google
