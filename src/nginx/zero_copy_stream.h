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
#ifndef NGINX_ZERO_COPY_STREAM_H_
#define NGINX_ZERO_COPY_STREAM_H_

#include <deque>
#include <memory>
#include <vector>

#include "contrib/endpoints/include/api_manager/utils/status.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "grpc++/support/byte_buffer.h"

extern "C" {
#include "src/http/ngx_http.h"
}

namespace google {
namespace api_manager {
namespace nginx {

// ::google::protobuf::io::ZeroCopyInputStream implementation over an NGINX
// request body needed by transcoding interface.
// Given an ngx_http_request_t* r this implementation will return the data in
// r->request_body->bufs (if there is data) until r->reading_body is false and
// all the buffers have been processed.
class NgxRequestZeroCopyInputStream
    : public ::google::protobuf::io::ZeroCopyInputStream {
 public:
  NgxRequestZeroCopyInputStream(ngx_http_request_t* r);

  // Reports the status in case of an error
  utils::Status Status() const { return status_; }

  // ZeroCopyInputStream implementation
  bool Next(const void** data, int* size);
  void BackUp(int count);
  bool Skip(int count) { return false; }  // not supported
  ::google::protobuf::int64 ByteCount() const;

 private:
  // Advances to the next buffer. Returns true if successful; otherwise returns
  // false (if no buffer is available or if an error occured).
  bool NextBuffer();

  // The request
  ngx_http_request_t* r_;

  // The current chain link
  ngx_chain_t* cl_;

  // The current buffer (might be different from cl_->buf in case cl_->buf is
  // file-based)
  ngx_buf_t* buf_;

  // The current position in the buffer
  u_char* pos_;

  utils::Status status_;
};

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // NGINX_ZERO_COPY_STREAM_H_
