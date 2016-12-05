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

#include "src/nginx/grpc_server_call.h"

#include <cassert>
#include <utility>

#include "contrib/endpoints/include/api_manager/utils/status.h"
#include "grpc++/support/byte_buffer.h"
#include "src/nginx/error.h"
#include "src/nginx/grpc_finish.h"
#include "src/nginx/module.h"
#include "src/nginx/util.h"

extern "C" {
#include "third_party/nginx/src/http/v2/ngx_http_v2_module.h"
}

namespace google {
namespace api_manager {
namespace nginx {

namespace {

// Returns a character at a logical offset within a vector of slices.
unsigned char GetByte(const std::vector<gpr_slice> &slices, size_t offset) {
  for (const auto &it : slices) {
    if (offset < GPR_SLICE_LENGTH(it)) {
      return GPR_SLICE_START_PTR(it)[offset];
    }
    offset -= GPR_SLICE_LENGTH(it);
  }
  return '\0';
}

// Trims some bytes from the front of vector of slices, removing
// slices as needed.
void TrimFront(std::vector<gpr_slice> *slices, size_t count) {
  while (count) {
    gpr_slice &head = slices->front();
    if (GPR_SLICE_LENGTH(head) <= count) {
      count -= GPR_SLICE_LENGTH(head);
      gpr_slice_unref(head);
      slices->erase(slices->begin());
    } else {
      head = gpr_slice_sub_no_ref(head, count, GPR_SLICE_LENGTH(head));
      break;
    }
  }
}

// Calls ngx_http_output_filter() to call the output filter chain and sets up
// the write event handler and the write event if ngx_http_output_filter()
// returns NGX_AGAIN.
ngx_int_t ngx_esp_write_output(ngx_http_request_t *r, ngx_chain_t *out,
                               ngx_http_event_handler_pt write_event_handler) {
  ngx_int_t rc = ngx_http_output_filter(r, out);

  if (rc == NGX_OK) {
    return NGX_OK;
  }

  if (rc != NGX_AGAIN) {
    return rc;
  }

  ngx_http_core_loc_conf_t *clcf = reinterpret_cast<ngx_http_core_loc_conf_t *>(
      ngx_http_get_module_loc_conf(r, ngx_http_core_module));

  // Set the r->write_event_handler and call ngx_handle_write_event() to listen
  // for write events. When the socket is ready for writing again, NGINX will
  // call r->write_event_handler.
  r->write_event_handler = write_event_handler;
  rc = ngx_handle_write_event(r->connection->write, clcf->send_lowat);
  if (rc != NGX_OK) {
    return rc;
  }

  return NGX_AGAIN;
}

}  // namespace

NgxEspGrpcServerCall::NgxEspGrpcServerCall(ngx_http_request_t *r,
                                           bool delay_downstream_headers)
    : r_(r),
      add_header_failed_(false),
      reading_(false),
      read_msg_(nullptr),
      delay_downstream_headers_(delay_downstream_headers) {
  // Add the cleanup handler.  This unlinks the NgxEspGrpcServerCall
  // from the request when the underlying nginx request is terminated,
  // since the NgxEspGrpcServerCall may outlive the request.
  cln_.handler = &NgxEspGrpcServerCall::Cleanup;
  cln_.data = this;
  cln_.next = r->cleanup;
  r->cleanup = &cln_;
}

utils::Status NgxEspGrpcServerCall::ProcessPrereadRequestBody() {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r_);
  ctx->grpc_server_call = this;
  // In the typical case, there will actually be a body to be passed
  // to upstream.  So begin the read from downstream immediately
  // (i.e. do not wait for the upstream connection to be established),
  // setting up a call to OnDownstreamPreread when preread data is
  // available.
  r_->request_body_no_buffering = 1;
  ngx_int_t rc = ngx_http_read_client_request_body(
      r_, &NgxEspGrpcServerCall::OnDownstreamPreread);

  if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                   "NgxEspGrpcServerCall: received special response %d", rc);
    return utils::Status(rc, utils::Status::CodeToString(rc));
  }

  return utils::Status::OK;
}

NgxEspGrpcServerCall::~NgxEspGrpcServerCall() {
  if (cln_.data) {
    for (ngx_http_cleanup_t **c = &r_->cleanup; *c != nullptr;
         c = &(*c)->next) {
      if (*c == &cln_) {
        *c = (*c)->next;
        break;
      }
    }
  }
  for (auto &slice : downstream_slices_) {
    gpr_slice_unref(slice);
  }
  downstream_slices_.clear();
}

void NgxEspGrpcServerCall::UpdateRequestMessageStat(int64_t size) {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r_);
  ctx->grpc_request_bytes += size;
  ++ctx->grpc_request_message_counts;
  ctx->request_handler->AttemptIntermediateReport();
}
void NgxEspGrpcServerCall::UpdateResponseMessageStat(int64_t size) {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r_);
  ctx->grpc_response_bytes += size;
  ++ctx->grpc_response_message_counts;
  ctx->request_handler->AttemptIntermediateReport();
}

void NgxEspGrpcServerCall::AddInitialMetadata(const std::string &key,
                                              const std::string &value) {
  if (!cln_.data) {
    return;
  }

  if (!value.length()) {
    return;
  }

  ngx_str_t ngkey;
  ngx_str_t ngval;

  if (ngx_str_copy_from_std(r_->pool, key, &ngkey) != NGX_OK) {
    add_header_failed_ = true;
    return;
  }

  if (ngx_str_copy_from_std(r_->pool, value, &ngval) != NGX_OK) {
    add_header_failed_ = true;
    return;
  }

  auto *h = reinterpret_cast<ngx_table_elt_t *>(
      ngx_list_push(&r_->headers_out.headers));
  if (!h) {
    add_header_failed_ = true;
    return;
  }

  h->hash = 1;
  h->key = ngkey;
  h->value = ngval;

  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                 "NgxEspGrpcServerCall::AddInitialMetadata: "
                 "%V: %V",
                 &ngkey, &ngval);
}

void NgxEspGrpcServerCall::SendInitialMetadata(
    std::multimap<std::string, std::string> initial_metadata,
    std::function<void(bool)> continuation) {
  if (!cln_.data) {
    continuation(false);
    return;
  }

  for (const auto &it : initial_metadata) {
    AddInitialMetadata(it.first, it.second);
  }

  if (add_header_failed_) {
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                   "NgxEspGrpcServerCall::SendInitialMetadata: "
                   "Adding headers failed");
    continuation(false);
    return;
  }

  if (delay_downstream_headers_) {
    // Do nothing for now.  If delay_downstream_headers_ is true, we delay
    // sending the headers to the client until the first response message or the
    // end of the request.
    //
    // This helps to propagate the errors reported by the backend using the HTTP
    // status code in the transcoding case. If the backend returns an error
    // status before the first message, we will be able to send it to the client
    // as an HTTP status code.
    continuation(true);
    return;
  }

  auto status = WriteDownstreamHeaders();
  continuation(status.ok());
}

utils::Status NgxEspGrpcServerCall::WriteDownstreamHeaders() {
  r_->headers_out.status = NGX_HTTP_OK;
  r_->headers_out.content_type = response_content_type();
  ngx_int_t rc = ngx_http_send_header(r_);
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                 "NgxEspGrpcServerCall::WriteDownstreamHeaders: "
                 "ngx_http_send_header: %i",
                 rc);
  if (rc == NGX_ERROR || rc > NGX_OK) {
    return utils::Status(rc, std::string());
  }
  return utils::Status::OK;
}

void NgxEspGrpcServerCall::OnDownstreamWriteable(ngx_http_request_t *r) {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r);
  NgxEspGrpcServerCall *server_call = ctx->grpc_server_call;

  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "NgxEspGrpcServerCall::OnDownstreamWriteable");

  ngx_int_t rc = ngx_esp_write_output(
      r, nullptr, &NgxEspGrpcServerCall::OnDownstreamWriteable);

  if (rc == NGX_AGAIN) {
    ngx_log_debug0(
        NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "NgxEspGrpcServerCall::OnDownstreamWriteable: Flush blocked");
    return;
  }

  if (!server_call) {
    return;
  }

  std::function<void(bool)> continuation;
  std::swap(continuation, server_call->write_continuation_);

  if (continuation) {
    bool ok = (rc == NGX_OK);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "NgxEspGrpcServerCall::OnDownstreamWriteable: Write %s",
                   ok ? "OK" : "NOT ok");
    continuation(ok);
  }
}

// This is the proxy's nginx post_handler (i.e. it's passed to
// ngx_http_read_client_request_body to get data flowing from nginx to
// this module)
// nginx introduced preread in http://hg.nginx.org/nginx/rev/ce94f07d5082
// so the preread data in r->request_body must be processed at this point
// before they are freed.
void NgxEspGrpcServerCall::OnDownstreamPreread(ngx_http_request_t *r) {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r);
  NgxEspGrpcServerCall *server_call = ctx->grpc_server_call;

  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "NgxEspGrpcServerCall::OnDownstreamPreread");

  // Convert the preread request body and store in downstream_slices_
  if (!server_call->ConvertRequestBody(&server_call->downstream_slices_)) {
    // Error occurred, ConvertRequestBody() has finalized the request, nothing
    // to do anymore.
    ngx_log_error(
        NGX_LOG_DEBUG, server_call->r_->connection->log, 0,
        "Failed to convert the preread request body for a gRPC call.");
    return;
  }

  r->read_event_handler = &ngx_http_block_reading;
}

// This is the proxy's nginx read_event_handler which will be invoked once
// there's data to be processed.
void NgxEspGrpcServerCall::OnDownstreamReadable(ngx_http_request_t *r) {
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r);
  NgxEspGrpcServerCall *server_call = ctx->grpc_server_call;

  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "NgxEspGrpcServerCall::OnDownstreamReadable");

  // Drive the read loop.
  server_call->RunPendingRead();
}

void NgxEspGrpcServerCall::Read(
    ::grpc::ByteBuffer *msg,
    std::function<void(bool, utils::Status)> continuation) {
  if (!cln_.data) {
    continuation(false, utils::Status::OK);
    return;
  }

  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                 "NgxEspGrpcServerCall::Read");

  assert(!read_continuation_);

  // Set the read pending.
  read_msg_ = msg;
  read_continuation_ = continuation;

  // We might already be reading (i.e. we may have invoked a
  // continuation that's calling back into this function).  If so, we
  // need to return, so that we don't blow our stack.
  if (reading_) {
    return;
  }

  // Drive the read loop.
  RunPendingRead();
}

void NgxEspGrpcServerCall::CompletePendingRead(bool proceed,
                                               utils::Status status) {
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                 "NgxEspGrpcServerCall::CompletePendingRead: %s",
                 proceed ? "OK" : "NOT ok");
  std::function<void(bool, utils::Status)> continuation;
  std::swap(continuation, read_continuation_);
  read_msg_ = nullptr;

  // We already received end stream flag from downstream (r_reading_body is 0)
  // and we converted all downstream_slices_ at this point, so it is ok to send
  // end stream to upstream.
  if (proceed && !r_->reading_body && downstream_slices_.size() == 0) {
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                   "NgxEspGrpcServerCall::CompletePendingRead: DONE");
    status = utils::Status::DONE;
  }
  continuation(proceed, status);
  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                 "NgxEspGrpcServerCall::CompletePendingRead: complete");
}

void NgxEspGrpcServerCall::RunPendingRead() {
  reading_ = true;
  bool try_read_unbuffered_request_body = true;

  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                 "NgxEspGrpcServerCall::RunPendingRead: Starting loop");

  while (read_continuation_) {
    // Read and convert the request body
    if (!ConvertRequestBody(&downstream_slices_)) {
      // Error occurred, ConvertRequestBody() has finalized the request and
      // Cleanup() has called the pending read continuation with ok=false.
      // Nothing to do anymore. Can't even log as the request has been freed.
      return;
    }
    // Attempt to read and complete a message from downstream.  Note
    // that the callback here might wind up enqueueing another read.
    if (TryReadDownstreamMessage()) {
      // We successfully read a message and completed the read.
      ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                     "NgxEspGrpcServerCall::RunPendingRead: Read a message");
      continue;
    }

    // Check whether the stream's half-closed.
    if (!r_->reading_body) {
      ngx_log_debug0(
          NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
          "NgxEspGrpcServerCall::RunPendingRead: Stream is half-closed");
      CompletePendingRead(false, utils::Status::OK);
      continue;
    }

    if (!try_read_unbuffered_request_body) {
      // At this point, there's not enough data to complete a GRPC
      // message, but the stream's still open, and we have a read
      // continuation callback available, so we can make progress once
      // there's something to read.  Since we've called
      // ngx_http_read_unbuffered_request_body and gotten nothing
      // back, there's nothing more we can do right now.
      // r_->read_event_handler will be invoked once there's something
      // to read; point the handler to our downstream traffic handler.
      ngx_log_debug0(
          NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
          "NgxEspGrpcServerCall::RunPendingRead: Setting read handler");
      r_->read_event_handler = &NgxEspGrpcServerCall::OnDownstreamReadable;
      break;
    }

    // At this point, we don't have enough data for a GRPC message,
    // but the connection's still open, we have a read continuation
    // callback, and we haven't tried actually reading more data from
    // the underlying stream.  So try to read more data.  As a side
    // effect, this will either clear r_->reading_body (because the
    // stream is already half-closed), or cause r_->read_event_handler
    // to be invoked at some point.
    ngx_log_debug0(
        NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
        "NgxEspGrpcServerCall::RunPendingRead: Reading unbuffered body");
    ngx_int_t rc = ngx_http_read_unbuffered_request_body(r_);
    if (rc != NGX_AGAIN && rc != NGX_OK) {
      ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                     "NgxEspGrpcServerCall::RunPendingRead: rc = %d", rc);
      // Reading (downstream) request body failed; finish writing to the
      // upstream and exit with an error status
      CompletePendingRead(false,
                          utils::Status(rc, utils::Status::CodeToString(rc)));
      continue;
    }
    try_read_unbuffered_request_body = false;
    for (ngx_chain_t *cl = r_->request_body->bufs; cl; cl = cl->next) {
      if (ngx_buf_size(cl->buf) > 0) {
        // Since ngx_read_unbuffered_request_body got something, it's
        // worth trying again.
        try_read_unbuffered_request_body = true;
        break;
      }
    }
    ngx_log_debug1(
        NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
        "NgxEspGrpcServerCall::RunPendingRead: try_read_unbuffered => %s",
        try_read_unbuffered_request_body ? "True" : "False");
  }

  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                 "NgxEspGrpcServerCall::RunPendingRead: Loop complete");

  reading_ = false;

  if (!read_continuation_ && cln_.data) {
    // Make sure to block subsequent downstream reads until there's a
    // continuation, so that flow control works correctly.
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                   "NgxEspGrpcServerCall::RunPendingRead: Blocking reads");
    r_->read_event_handler = &ngx_http_block_reading;
  }
}

bool NgxEspGrpcServerCall::TryReadDownstreamMessage() {
  // From http://www.grpc.io/docs/guides/wire.html, a GRPC message is:
  // * A one-byte compressed-flag
  // * A four-byte message length
  // * The message body (length octets)

  size_t buflen = 0;
  for (auto it : downstream_slices_) {
    buflen += GPR_SLICE_LENGTH(it);
  }
  ngx_log_debug1(
      NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
      "NgxEspGrpcServerCall::TryReadDownstreamMessage: buffered length=%z",
      buflen);

  if (buflen < 5) {
    // There's not even enough data to figure out how long the message
    // is.
    return false;
  }

  // TODO: Implement compressed-flag

  // Decode the length.  Note that this is in network byte order.
  uint32_t msglen = 0;
  msglen |= GetByte(downstream_slices_, 1);
  msglen <<= 8;
  msglen |= GetByte(downstream_slices_, 2);
  msglen <<= 8;
  msglen |= GetByte(downstream_slices_, 3);
  msglen <<= 8;
  msglen |= GetByte(downstream_slices_, 4);

  ngx_log_debug1(
      NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
      "NgxEspGrpcServerCall::TryReadDownstreamMessage: message length=%z",
      msglen);

  if ((buflen - 5) < msglen) {
    // We're still waiting for the rest of the message.
    return false;
  }

  // Okay, we can return a message.
  TrimFront(&downstream_slices_, 5);

  // Get an iterator pointing into the downstream_slices_ vector to the slice
  // containing the byte just pass the message length (which could be
  // downstream_slices_->end() if the length is an exact match).
  auto it = downstream_slices_.begin();
  size_t prefixlen =
      0;  // The number of bytes in downstream_slices_ before *it.
  while (prefixlen < msglen && GPR_SLICE_LENGTH(*it) <= (msglen - prefixlen)) {
    prefixlen += GPR_SLICE_LENGTH(*it);
    ++it;
  }

  gpr_slice remainder;
  if (prefixlen < msglen) {
    // We need to use part of the contents of the slice at *it, but
    // not all of the contents.  Save the remainder off on the side
    // (with its own refcount), replace the vector element with the
    // trimmed slice (i.e. what we want for building the message byte
    // buffer), and advance to the next slice.  After this, the slices
    // [downstream_slices_->begin(), it) are the message contents.
    remainder = gpr_slice_sub(*it, msglen - prefixlen, GPR_SLICE_LENGTH(*it));
    *it = gpr_slice_sub_no_ref(*it, 0, msglen - prefixlen);
    ++it;
  }

  // Copy the slices, logically transferring their reference counts to
  // the 'slices' vector (which will drop those reference counts as
  // the vector is destroyed).
  std::vector<::grpc::Slice> slices;
  slices.reserve(it - downstream_slices_.begin());
  std::transform(downstream_slices_.begin(), it, std::back_inserter(slices),
                 [](gpr_slice &slice) {
                   return ::grpc::Slice(slice, ::grpc::Slice::STEAL_REF);
                 });

  // Write the message byte buffer (giving the ByteBuffer its own
  // reference counts).
  *read_msg_ = ::grpc::ByteBuffer(slices.data(), slices.size());

  if (prefixlen < msglen) {
    // Replace the last slice used to create the byte buffer with the
    // remainder after the message contents.
    --it;
    *it = remainder;
  }

  // Erase the consumed slices.
  downstream_slices_.erase(downstream_slices_.begin(), it);

  // Complete the pending Read operation.
  CompletePendingRead(true, utils::Status::OK);

  // Indicate to our caller that we were able to complete a message.
  return true;
}

void NgxEspGrpcServerCall::Write(const ::grpc::ByteBuffer &msg,
                                 std::function<void(bool)> continuation) {
  if (!cln_.data) {
    continuation(false);
    return;
  }

  // Make sure the headers have been sent
  if (!r_->header_sent) {
    auto status = WriteDownstreamHeaders();
    if (!status.ok()) {
      ngx_log_error(NGX_LOG_DEBUG, r_->connection->log, 0,
                    "Faield to send the headers");
      continuation(false);
      return;
    }
  }

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                 "NgxEspGrpcServerCall::Write: Writing %z bytes", msg.Length());

  ngx_chain_t out;
  if (!ConvertResponseMessage(msg, &out)) {
    // Converting the response message failed. ConvertResponseMessage() has
    // finalized the request, call the continuation with false to abort the
    // call.
    ngx_log_error(NGX_LOG_DEBUG, r_->connection->log, 0,
                  "Failed to convert a gRPC response message.");
    continuation(false);
    return;
  }

  ngx_int_t rc = ngx_esp_write_output(
      r_, &out, &NgxEspGrpcServerCall::OnDownstreamWriteable);

  if (rc == NGX_OK) {
    // We were immediately able to send the message downstream.
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                   "NgxEspGrpcServerCall::Write: completed synchronously");
    continuation(true);
    return;
  }

  if (rc != NGX_AGAIN) {
    // We failed to send the message downstream.
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                   "NgxEspGrpcServerCall::Write: failed synchronously");
    continuation(false);
    return;
  }

  // Otherwise: the message is in the outgoing queue.
  write_continuation_ = continuation;
  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
                 "NgxEspGrpcServerCall::Write: blocked");
}

void NgxEspGrpcServerCall::RecordBackendTime(int64_t backend_time) {
  if (!cln_.data) {
    return;
  }
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r_);
  if (ctx != nullptr) {
    ctx->backend_time = backend_time;
  }
}

void NgxEspGrpcServerCall::Cleanup(void *server_call_ptr) {
  if (!server_call_ptr) {
    return;
  }
  auto server_call = reinterpret_cast<NgxEspGrpcServerCall *>(server_call_ptr);
  if (server_call->read_continuation_) {
    server_call->CompletePendingRead(false, utils::Status::OK);
  }
  server_call->cln_.data = nullptr;
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
