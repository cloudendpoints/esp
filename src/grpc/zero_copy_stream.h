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
#ifndef GRPC_ZERO_COPY_STREAM_H_
#define GRPC_ZERO_COPY_STREAM_H_

#include <deque>

#include "contrib/endpoints/src/grpc/transcoding/transcoder_input_stream.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "grpc++/support/byte_buffer.h"
#include "src/grpc/message_serializer.h"

namespace google {
namespace api_manager {
namespace grpc {

// ZeroCopyInputStream implementation over a stream of gRPC messages.
class GrpcZeroCopyInputStream
    : public ::google::api_manager::transcoding::TranscoderInputStream {
 public:
  GrpcZeroCopyInputStream();

  // Add a message to the end of the stream
  void AddMessage(grpc_byte_buffer* message, bool take_ownership);

  // Marks the end of the stream, which means that ZeroCopyInputStream will
  // return false after all the existing messages are consumed.
  void Finish() { finished_ = true; }

  // ZeroCopyInputStream implementation

  bool Next(const void** data, int* size);
  void BackUp(int count);
  bool Skip(int count) { return false; }                     // not supported
  ::google::protobuf::int64 ByteCount() const { return 0; }  // Not implemented
  int64_t BytesAvailable() const;

 private:
  GrpcMessageSerializer serializer_;
  const unsigned char* current_buffer_;
  size_t current_buffer_size_;
  size_t position_;
  bool finished_;
};

}  // namespace grpc
}  // namespace api_manager
}  // namespace google

#endif  // GRPC_ZERO_COPY_STREAM_H_
