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
#include "src/nginx/zero_copy_stream.h"

extern "C" {
#include "src/http/ngx_http.h"
}

namespace google {
namespace api_manager {
namespace nginx {

namespace {

// Read a file-based ngx_buf_t into a memory-based ngx_buf_t
ngx_buf_t* ReadFileBuffer(ngx_pool_t* pool, ngx_buf_t* file_buf) {
  // TODO: If the file is too large, read it in chunks.
  auto file_size = ngx_buf_size(file_buf);
  auto buf = ngx_create_temp_buf(pool, file_size);
  if (!buf) {
    // Failed to allocate a buffer, return nullptr to indicate an error.
    return nullptr;
  }

  // If the buffer's not in memory, we need to read the contents.
  if (NGX_ERROR == ngx_read_file(file_buf->file, buf->pos,
                                 ngx_buf_size(file_buf), file_buf->file_pos)) {
    // Error could not read the file.
    return nullptr;
  }

  // Set the size correctly
  buf->last = buf->pos + file_size;

  return buf;
}

bool IsEmptyBuffer(ngx_buf_t* buf) { return !buf || 0 == ngx_buf_size(buf); }

ngx_chain_t* FindNonEmptyChainLink(ngx_chain_t* cl) {
  // Try to find the next chain link with non-empty buffer
  while (cl && IsEmptyBuffer(cl->buf)) {
    cl = cl->next;
  }
  return cl;
}

}  // namesapce

NgxRequestZeroCopyInputStream::NgxRequestZeroCopyInputStream(
    ngx_http_request_t* r)
    : r_(r), cl_(nullptr), buf_(nullptr), pos_(0), status_(utils::Status::OK) {}

bool NgxRequestZeroCopyInputStream::Next(const void** data, int* size) {
  if (!status_.ok()) {
    return false;
  }

  if (!buf_ || pos_ >= buf_->last) {
    // Either we don't have a current buffer or it is exhausted. Advance to the
    // next buffer.
    if (!NextBuffer()) {
      *size = 0;
      // If there was an error or r_->reading_body is false (which means NGINX
      // will not read more data), return false to indicate error or end of
      // data. Otherwise, return true to indicate that there is no data at the
      // moment, but there might be more.
      return status_.ok() && r_->reading_body;
    }
  }
  // We have a non-empty buffer at this point.
  *data = pos_;
  *size = static_cast<int>(buf_->last - pos_);

  ngx_log_debug1(
      NGX_LOG_DEBUG_HTTP, r_->connection->log, 0,
      "NgxRequestZeroCopyInputStream: Next => %s",
      std::string(reinterpret_cast<const char*>(*data), *size).c_str());

  // Advance the positions
  //  - advance buf_->pos to mark everything before buf_->last consumed
  //  - advance pos_ to buf_->last as we are returning the remaining buffer
  buf_->pos = buf_->last;
  pos_ = buf_->last;

  return true;
}

void NgxRequestZeroCopyInputStream::BackUp(int count) {
  if (buf_ && 0 < count && count <= pos_ - buf_->pos) {
    pos_ -= count;
  }
}

::google::protobuf::int64 NgxRequestZeroCopyInputStream::ByteCount() const {
  if (!status_.ok()) {
    return 0;
  }

  // Bytes left in the current buffer
  auto total = buf_ ? (buf_->last - pos_) : 0;

  // Bytes left in the subsequent buffers
  auto cl = cl_ ? cl_->next : nullptr;
  while (cl) {
    total += ngx_buf_size(cl->buf);
    cl = cl->next;
  }

  return static_cast<::google::protobuf::int64>(total);
}

bool NgxRequestZeroCopyInputStream::NextBuffer() {
  // Free the current buffer before moving on to the next one
  if (cl_ && cl_->buf && !ngx_buf_in_memory(cl_->buf)) {
    // This was our temp buffer, so free it.
    ngx_pfree(r_->pool, buf_->start);
    // Also, mark the file as consumed.
    cl_->buf->file_pos = cl_->buf->file_last;
  }

  // Find the next non-empty buffer. If this is the first buffer, start with
  // r_->request_body->bufs.
  cl_ = FindNonEmptyChainLink(cl_ ? cl_->next : r_->request_body->bufs);
  if (!cl_) {
    // No data available
    return false;
  }

  if (!ngx_buf_in_memory(cl_->buf)) {
    // A file buffer - read it into memory.
    buf_ = ReadFileBuffer(r_->pool, cl_->buf);
    if (!buf_) {
      ngx_log_error(NGX_LOG_ERR, r_->connection->log, 0,
                    "Failed to read the file buffer.");
      status_ = utils::Status(NGX_HTTP_INTERNAL_SERVER_ERROR,
                              "Internal error reading the request data.");
      return false;
    }
  } else {
    // In-memory buffer, so we can use it as-is.
    buf_ = cl_->buf;
  }
  pos_ = buf_->pos;
  return true;
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
