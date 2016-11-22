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
#include "src/nginx/grpc/message_serializer.h"

#include "grpc++/support/byte_buffer.h"

namespace google {
namespace api_manager {
namespace grpc {

GrpcMessageSerializer::GrpcMessageSerializer()
    : delimiter_consumed_(false), current_slice_no_(0), byte_count_(0) {}

GrpcMessageSerializer::~GrpcMessageSerializer() {
  while (!messages_.empty()) {
    if (messages_.front().second) {
      grpc_byte_buffer_destroy(messages_.front().first);
    }
    messages_.pop_front();
  }
}

void GrpcMessageSerializer::AddMessage(grpc_byte_buffer* message,
                                       bool take_ownership) {
  messages_.emplace_back(message, take_ownership);

  // Adding the message size + 5 bytes for the delimiter.
  byte_count_ += grpc_byte_buffer_length(message) + kMessageDelimiterSize;
}

bool GrpcMessageSerializer::Next(const unsigned char** data, size_t* size) {
  // Call NextInternal() until it returns false or a non-empty buffer.
  while (NextInternal(data, size)) {
    if (*size != 0) {
      return true;
    }
  }
  return false;
}

bool GrpcMessageSerializer::NextInternal(const unsigned char** data,
                                         size_t* size) {
  if (messages_.empty()) {
    return false;
  }

  // Check that if we have exhausted the current buffer.
  if (delimiter_consumed_ &&
      current_slice_no_ >=
          messages_.front().first->data.raw.slice_buffer.count) {
    // The current buffer has been consumed, destroy it if needed
    if (messages_.front().second) {
      // Destroy it if the ownership was transfered to GrpcMessageSerializer
      grpc_byte_buffer_destroy(messages_.front().first);
    }
    messages_.pop_front();

    // Reset the state for processing the next buffer
    delimiter_consumed_ = false;
    current_slice_no_ = 0;
  }

  // If there are no more buffers, return false
  if (messages_.empty()) {
    return false;
  }

  if (!delimiter_consumed_) {
    // The delimiter has not been consumed, so return the delimiter first.
    //
    // From http://www.grpc.io/docs/guides/wire.html, a GRPC message delimiter
    // is:
    // * A one-byte compressed-flag
    // * A four-byte message length
    unsigned int msglen = static_cast<unsigned int>(
        grpc_byte_buffer_length(messages_.front().first));
    delimiter_[0] =
        (messages_.front().first->data.raw.compression == GRPC_COMPRESS_NONE
             ? 0
             : 1);
    delimiter_[4] = msglen & 0xFF;
    msglen >>= 8;
    delimiter_[3] = msglen & 0xFF;
    msglen >>= 8;
    delimiter_[2] = msglen & 0xFF;
    msglen >>= 8;
    delimiter_[1] = msglen & 0xFF;

    *data = delimiter_;
    *size = kMessageDelimiterSize;

    delimiter_consumed_ = true;
    current_slice_no_ = 0;
    byte_count_ -= kMessageDelimiterSize;
    return true;
  }

  // At this point we know that the delimiter has been consumed and we must
  // have a regular slice to return.
  auto slice_buffer = &messages_.front().first->data.raw.slice_buffer;
  *data = GPR_SLICE_START_PTR(slice_buffer->slices[current_slice_no_]);
  *size = static_cast<int>(
      GPR_SLICE_LENGTH(slice_buffer->slices[current_slice_no_]));
  ++current_slice_no_;
  byte_count_ -= *size;
  return true;
}

}  // namespace grpc
}  // namespace api_manager
}  // namespace google
