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
#ifndef GRPC_MESSAGE_SERIALIZER_H_
#define GRPC_MESSAGE_SERIALIZER_H_

#include <deque>

#include "grpc++/support/byte_buffer.h"

namespace google {
namespace api_manager {
namespace grpc {

// Serializes discrete gRPC messages (in serialized form) by joining them via
// the message delimiters. Also, abstracts away the low-level grpc-byte_buffer
// interface.
class GrpcMessageSerializer {
 public:
  GrpcMessageSerializer();

  // Adds a gRPC message
  // take_ownership - determines whether the ownership of the message is passed
  //                  to the GrpcMessageSerializer or remains with the caller.
  //                  This is the flag returned by gRPC when we convert
  //                  ::grpc::ByteBuffer to grpc_byte_buffer using
  //                  SerializationTraits<ByteBuffer, void >::Serialize().
  //                  With the current gRPC implementation it's always true.
  void AddMessage(grpc_byte_buffer* message, bool take_ownership);

  // Returns the next non-empty buffer of the serialized gRPC messages.
  bool Next(const unsigned char** data, size_t* size);

  // Returns the total number of bytes available for Next() to return until all
  // the messages added via AddMessage() are exhausted.
  size_t ByteCount() const { return byte_count_; }

 private:
  // Returns the next buffer of the serialized gRPC messages. Unlike the public
  // Next() function this might return empty buffer (i.e. *size == 0).
  bool NextInternal(const unsigned char** data, size_t* size);

  static const size_t kMessageDelimiterSize = 5;

  // Memory for message delimiters.
  unsigned char delimiter_[kMessageDelimiterSize];

  // The queue of the messages to be serialized. With each message we keep a
  // boolean flag that indicates whether the ownership is with
  // GrpcMessageSerializer or not.
  std::deque<std::pair<grpc_byte_buffer*, bool>> messages_;

  // Whether the delimiter of the current message was consumed or not
  bool delimiter_consumed_;

  // Which slice of the current message are we processing
  size_t current_slice_no_;

  // The number of unprocessed bytes
  size_t byte_count_;
};

}  // namespace grpc
}  // namespace api_manager
}  // namespace google

#endif  // GRPC_MESSAGE_SERIALIZER_H_
