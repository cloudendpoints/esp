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
#include "src/grpc/zero_copy_stream.h"

#include "grpc++/support/byte_buffer.h"

namespace google {
namespace api_manager {
namespace grpc {

GrpcZeroCopyInputStream::GrpcZeroCopyInputStream()
    : current_buffer_(nullptr),
      current_buffer_size_(0),
      position_(0),
      bytes_read_(0),
      finished_(false) {}

void GrpcZeroCopyInputStream::AddMessage(grpc_byte_buffer* message,
                                         bool take_ownership) {
  serializer_.AddMessage(message, take_ownership);
}

bool GrpcZeroCopyInputStream::Next(const void** data, int* size) {
  if (position_ >= current_buffer_size_) {
    bytes_read_ += current_buffer_size_;
    position_ = 0;
    if (!serializer_.Next(&current_buffer_, &current_buffer_size_)) {
      // No data
      *size = 0;
      current_buffer_size_ = 0;
      return !finished_;
    }
  }

  // Return [position_, current_buffer_size_) interval of the current buffer
  *data = current_buffer_ + position_;
  *size = static_cast<int>(current_buffer_size_ - position_);

  // Move the position to the end of the current buffer
  position_ = current_buffer_size_;
  return true;
}

void GrpcZeroCopyInputStream::BackUp(int count) {
  if (0 < count && static_cast<size_t>(count) <= position_) {
    position_ -= count;
  }
}

bool GrpcZeroCopyInputStream::Skip(int count) {
  if (count < 0) {
    // Safe guard against wrong usage.
    return false;
  }
  size_t count_left = static_cast<size_t>(count);
  while (position_ + count_left > current_buffer_size_) {
    // Skipping past the current buffer, read the next one.
    count_left -= current_buffer_size_ - position_;
    bytes_read_ += current_buffer_size_;
    position_ = 0;
    if (!serializer_.Next(&current_buffer_, &current_buffer_size_)) {
      // No data. We are potentially not at the end of the stream yet, but we
      // don't know that and can only skip to the end and return an error.
      current_buffer_size_ = 0;
      return false;
    }
  }

  // Move the position ahead the requested number of bytes.
  position_ += count_left;
  return true;
}

::google::protobuf::int64 ByteCount() const {
  return static_cast<::google::protobuf::int64>(bytes_passed_ + position_);
}

int64_t GrpcZeroCopyInputStream::BytesAvailable() const {
  return (current_buffer_size_ - position_) + serializer_.ByteCount();
}

}  // namespace grpc
}  // namespace api_manager
}  // namespace google
