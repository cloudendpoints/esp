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

namespace google {
namespace api_manager {
namespace nginx {

namespace {
const ngx_str_t kGrpcStatusHeaderKey = ngx_string("grpc-status");
const ngx_str_t kGrpcMessageHeaderKey = ngx_string("grpc-message");

// N.B. The following four functions are a direct copy of the
// corresponding functions from ngx_http_v2_filter_module.c -- since
// the originals are static, there's no good way to invoke them
// directly.

ngx_inline void ngx_esp_http_v2_handle_frame(ngx_http_v2_stream_t *stream,
                                             ngx_http_v2_out_frame_t *frame) {
  ngx_http_request_t *r;

  r = stream->request;

  r->connection->sent += NGX_HTTP_V2_FRAME_HEADER_SIZE + frame->length;

  if (frame->fin) {
    stream->out_closed = 1;
  }

  frame->next = stream->free_frames;
  stream->free_frames = frame;

  stream->queued--;
}

ngx_inline void ngx_esp_http_v2_handle_stream(ngx_http_v2_connection_t *h2c,
                                              ngx_http_v2_stream_t *stream) {
  ngx_event_t *wev;

  if (stream->handled || stream->blocked || stream->exhausted) {
    return;
  }

  wev = stream->request->connection->write;

  if (!wev->delayed) {
    stream->handled = 1;
    ngx_queue_insert_tail(&h2c->posted, &stream->queue);
  }
}

ngx_int_t ngx_esp_http_v2_headers_frame_handler(
    ngx_http_v2_connection_t *h2c, ngx_http_v2_out_frame_t *frame) {
  ngx_chain_t *cl, *ln;
  ngx_http_v2_stream_t *stream;

  stream = frame->stream;
  cl = frame->first;

  for (;;) {
    if (cl->buf->pos != cl->buf->last) {
      frame->first = cl;

      ngx_log_debug2(NGX_LOG_DEBUG_HTTP, h2c->connection->log, 0,
                     "http2:%ui HEADERS frame %p was sent partially",
                     stream->node->id, frame);

      return NGX_AGAIN;
    }

    ln = cl->next;

    if (cl->buf->tag == (ngx_buf_tag_t)&ngx_http_v2_module) {
      cl->next = stream->free_frame_headers;
      stream->free_frame_headers = cl;

    } else {
      cl->next = stream->free_bufs;
      stream->free_bufs = cl;
    }

    if (cl == frame->last) {
      break;
    }

    cl = ln;
  }

  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, h2c->connection->log, 0,
                 "http2:%ui HEADERS frame %p was sent", stream->node->id,
                 frame);

  ngx_esp_http_v2_handle_frame(stream, frame);

  ngx_esp_http_v2_handle_stream(h2c, stream);

  return NGX_OK;
}

u_char *ngx_esp_http_v2_write_int(u_char *pos, ngx_uint_t prefix,
                                  ngx_uint_t value) {
  if (value < prefix) {
    *pos++ |= value;
    return pos;
  }

  *pos++ |= prefix;
  value -= prefix;

  while (value >= 128) {
    *pos++ = value % 128 + 128;
    value /= 128;
  }

  *pos++ = (u_char)value;

  return pos;
}

ngx_int_t GrpcFinishV2(
    ngx_http_request_t *r, const utils::Status &status,
    std::multimap<std::string, std::string> response_trailers) {
  // This bit is a little hacky.
  //
  // The problem is that nginx's HTTP/2 stream implementation does not
  // (as of this writing) support the notion of trailed headers.  This
  // is unfortunate because GRPC reports the final API call status as
  // a trailed header.
  //
  // So if the request is HTTP/2, here's what we do: we directly
  // create an HTTP/2 headers frame (ala
  // ngx_http_v2_create_headers_frame), and then queue it to
  // r->stream->connection (ala ngx_http_v2_queue_blocked_frame) --
  // basically mirroring ngx_http_v2_header_filter.
  //
  // (We considered clearing r->header_sent and doing a normal header
  // send, but that would also send all of the HTTP headers normally
  // sent by nginx; there's no good way to skip them.)
  //
  // See rfc7541 for the header frame format details.  We encode
  // headers using uncompressed, unindexed values, since we don't want
  // to have to get into nginx's notion of the HPACK compression
  // state.
  //
  // TODO: Add the GRPC headers to the client's dynamic index.
  //
  // For now, according to the RFC section 6.2.2, each header is
  // represented as:
  //
  //      0   1   2   3   4   5   6   7
  //    +---+---+---+---+---+---+---+---+
  //    | 0 | 0 | 0 | 0 |       0       |
  //    +---+---+-----------------------+
  //    | H |     Name Length (7+)      |
  //    +---+---------------------------+
  //    |  Name String (Length octets)  |
  //    +---+---------------------------+
  //    | H |     Value Length (7+)     |
  //    +---+---------------------------+
  //    | Value String (Length octets)  |
  //    +-------------------------------+

  // TODO: Include any other trailers supplied by the upstream server.

  size_t length = 1 + /* 0 == header representation type */
                  2 * NGX_HTTP_V2_INT_OCTETS + /* name len, value len */
                  kGrpcStatusHeaderKey.len + NGX_INT_T_LEN + 1 /* code+sign */;
  bool add_status_details = false;
  size_t status_details_len = 0;
  if (!status.ok()) {
    add_status_details = true;
    status_details_len = status.message().length();
    if (status_details_len > NGX_HTTP_V2_MAX_FIELD) {
      status_details_len = NGX_HTTP_V2_MAX_FIELD;
    }
    length += 1 + 2 * NGX_HTTP_V2_INT_OCTETS + /* 0, name len, value len */
              kGrpcMessageHeaderKey.len + status_details_len;
  }

  // Build a headers frame.
  ngx_http_v2_out_frame_t *frame = new (r->pool) ngx_http_v2_out_frame_t;
  frame->handler = ngx_esp_http_v2_headers_frame_handler;
  frame->stream = r->stream;
  frame->blocked = 1;
  frame->fin = 1;
  ngx_buf_t *b =
      ngx_create_temp_buf(r->pool, NGX_HTTP_V2_FRAME_HEADER_SIZE + length);
  if (!b) {
    // There's very little we can do if we fail to allocate this
    // buffer; just finalize the request.
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  // Skip the frame prefix; we'll fill it in once we actually know
  // how big the frame is.  Note that b->start == b->last here, so we
  // have a pointer to the prefix.
  b->last += sizeof(uint32_t);

  // Write flags and stream ID.
  *b->last++ = (NGX_HTTP_V2_END_HEADERS_FLAG | NGX_HTTP_V2_END_STREAM_FLAG);
  b->last = ngx_http_v2_write_sid(b->last, r->stream->node->id);

  // Write the grpc-status header.
  *b->last++ = 0x0;
  *b->last++ = kGrpcStatusHeaderKey.len;
  b->last =
      ngx_cpymem(b->last, kGrpcStatusHeaderKey.data, kGrpcStatusHeaderKey.len);
  u_char *len = b->last++;
  b->last = ngx_sprintf(b->last, "%i", status.CanonicalCode());
  *len = b->last - len - 1;

  if (add_status_details) {
    // Write the grpc-message header.
    *b->last++ = 0x0;
    *b->last++ = kGrpcMessageHeaderKey.len;
    b->last = ngx_cpymem(b->last, kGrpcMessageHeaderKey.data,
                         kGrpcMessageHeaderKey.len);
    *b->last = 0x0;
    b->last = ngx_esp_http_v2_write_int(b->last, ngx_http_v2_prefix(7),
                                        status_details_len);
    b->last = ngx_copy(b->last, status.message().c_str(), status_details_len);
  }

  // Fill in the frame length and header.
  frame->length = b->last - b->start - NGX_HTTP_V2_FRAME_HEADER_SIZE;
  ngx_http_v2_write_len_and_type(b->start, frame->length,
                                 NGX_HTTP_V2_HEADERS_FRAME);

  // This is the last buffer in the chain.
  b->last_buf = 1;

  ngx_chain_t *cl = ngx_alloc_chain_link(r->pool);
  if (!cl) {
    // There's very little we can do if we fail to allocate this
    // buffer; just finalize the request.  Note that the temporary
    // buffer was allocated out of the request's pool -- there's no
    // need to free it.
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }
  cl->buf = b;
  frame->first = cl;
  frame->last = cl;

  // Queue the headers frame.
  ngx_http_v2_queue_blocked_frame(r->stream->connection, frame);
  r->stream->queued++;

  // Send the pending output queue (if it's not already moving).
  ngx_int_t rc = ngx_http_v2_send_output_queue(r->stream->connection);
  if (rc != NGX_OK) {
    return rc;
  }

  return NGX_DONE;
}

ngx_int_t GrpcFinishV1(
    ngx_http_request_t *r, const utils::Status &status,
    std::multimap<std::string, std::string> response_trailers) {
  // This bit is a little hacky.
  //
  // The problem is that nginx's HTTP/1.1 chunked reply implementation
  // does not (as of this writing) support the notion of trailed
  // headers.  This is unfortunate because GRPC reports the final API
  // call status as a trailed header.
  //
  // So if the request is HTTP/1.1, here's what we do: we reach in and
  // turn off the chunked bit in the request structure (which for some
  // interesting reason is also the reply structure).  This will cause
  // ngx_http_chunked_body_filter to ignore our output -- so our
  // output won't be wrapped in a chunk.  Then, we write the chunk
  // ourselves: it's a zero-length chunk with trailed headers.
  //
  // TODO: Implement and test HTTP 1.1 support.
  return NGX_DONE;
}

}  // namespace

ngx_int_t GrpcFinish(
    ngx_http_request_t *r, const utils::Status &status,
    std::multimap<std::string, std::string> response_trailers) {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r);
  if (ctx != nullptr) {
    ctx->check_status = status;
  }

  std::string status_str = status.ToString();
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "GrpcFinish: %s",
                 status_str.c_str());

  if (r->stream) {
    return GrpcFinishV2(r, status, response_trailers);
  } else {
    return GrpcFinishV1(r, status, response_trailers);
  }
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
