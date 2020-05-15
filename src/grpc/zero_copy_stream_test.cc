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

#include <string>
#include <vector>

#include "grpc++/support/byte_buffer.h"
#include "gtest/gtest.h"

namespace google {
namespace api_manager {
namespace grpc {
namespace testing {
namespace {

class GrpcZeroCopyInputStreamTest : public ::testing::Test {
public:
  GrpcZeroCopyInputStreamTest() {}
};

typedef std::vector<std::string> SliceData;

grpc_byte_buffer *CreateByteBuffer(const SliceData &slices) {
  std::vector<grpc_slice> grpc_slices;
  std::transform(std::begin(slices), std::end(slices),
                 std::back_inserter(grpc_slices), [](const std::string &slice) {
                   return grpc_slice_from_copied_buffer(&slice[0],
                                                        slice.size());
                 });
  auto out =
      grpc_raw_byte_buffer_create(grpc_slices.data(), grpc_slices.size());
  for (auto &slice : grpc_slices) {
    grpc_slice_unref(slice);
  }
  return out;
}

unsigned DelimiterToSize(const unsigned char *delimiter) {
  unsigned size = 0;
  // Bytes 1-4 are big-endian 32-bit message size
  size = size | static_cast<unsigned>(delimiter[1]);
  size <<= 8;
  size = size | static_cast<unsigned>(delimiter[2]);
  size <<= 8;
  size = size | static_cast<unsigned>(delimiter[3]);
  size <<= 8;
  size = size | static_cast<unsigned>(delimiter[4]);
  return size;
}

TEST_F(GrpcZeroCopyInputStreamTest, SimpleRead) {
  GrpcZeroCopyInputStream stream;

  // Check that there is no data right now
  const void *data = nullptr;
  int size = -1;
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(0, size);

  // Create and add a messages
  // The message has to be larger than grpc_slice inline size (20 bytes)
  std::string slice11 = "000000000000000000000This is\n";
  std::string slice12 = "000000000000000000000the first message\n";
  std::string slice21 = "000000000000000000000This is\n";
  std::string slice22 = "000000000000000000000the second message\n";

  stream.AddMessage(CreateByteBuffer(SliceData{slice11, slice12}), true);
  stream.AddMessage(CreateByteBuffer(SliceData{slice21, slice22}), true);
  stream.Finish();

  // Test BytesAvailable() and ByteCount()
  EXPECT_EQ(slice11.size() + slice12.size() + slice21.size() + slice22.size() +
                10, // +10 bytes for two delimiters
            stream.BytesAvailable());
  EXPECT_EQ(0, stream.ByteCount());

  // Test the message1 delimiter
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(5, size);
  EXPECT_EQ(slice11.size() + slice12.size(),
            DelimiterToSize(reinterpret_cast<const unsigned char *>(data)));

  // Test BytesAvailable() and ByteCount()
  EXPECT_EQ(slice11.size() + slice12.size() + slice21.size() + slice22.size() +
                5, // +5 bytes for one delimiter
            stream.BytesAvailable());
  EXPECT_EQ(5, stream.ByteCount());

  // Test the slices
  EXPECT_TRUE(stream.Next(&data, &size));
  //  EXPECT_EQ(slice11.size(), size);
  EXPECT_EQ(slice11, std::string(reinterpret_cast<const char *>(data), size));

  // Test BytesAvailable() and ByteCount()
  EXPECT_EQ(slice12.size() + slice21.size() + slice22.size() +
                5, // +5 bytes for one delimiter
            stream.BytesAvailable());
  EXPECT_EQ(5 + slice11.size(), stream.ByteCount());

  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(slice12.size(), size);
  EXPECT_EQ(slice12, std::string(reinterpret_cast<const char *>(data), size));

  // Test BytesAvailable() and ByteCount()
  EXPECT_EQ(slice21.size() + slice22.size() + 5, // +5 bytes for one delimiter
            stream.BytesAvailable());
  EXPECT_EQ(5 + slice11.size() + slice12.size(), stream.ByteCount());

  // Test the message2 delimiter
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(5, size);
  EXPECT_EQ(slice21.size() + slice22.size(),
            DelimiterToSize(reinterpret_cast<const unsigned char *>(data)));

  // Test BytesAvailable() and ByteCount()
  EXPECT_EQ(slice21.size() + slice22.size(), stream.BytesAvailable());
  EXPECT_EQ(5 + slice11.size() + slice12.size() + 5, stream.ByteCount());

  // Test the slices
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(slice21.size(), size);
  EXPECT_EQ(slice21, std::string(reinterpret_cast<const char *>(data), size));

  // Test BytesAvailable() and ByteCount()
  EXPECT_EQ(slice22.size(), stream.BytesAvailable());
  EXPECT_EQ(5 + slice11.size() + slice12.size() + 5 + slice21.size(),
            stream.ByteCount());

  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(slice22.size(), size);
  EXPECT_EQ(slice22, std::string(reinterpret_cast<const char *>(data), size));

  // Test the end of the stream
  EXPECT_EQ(0, stream.BytesAvailable());
  EXPECT_EQ(5 + slice11.size() + slice12.size() + 5 + slice21.size() +
                slice22.size(),
            stream.ByteCount());
  EXPECT_FALSE(stream.Next(&data, &size));
}

TEST_F(GrpcZeroCopyInputStreamTest, Backups) {
  GrpcZeroCopyInputStream stream;

  // Create and add a message
  std::string slice1 = "This is the first slice of the message.";
  std::string slice2 = "This is the second slice of the message.";

  stream.AddMessage(CreateByteBuffer(SliceData{slice1, slice2}), true);
  stream.Finish();

  // Check the message delimiter
  const void *data = nullptr;
  int size = -1;
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(5, size);
  EXPECT_EQ(slice1.size() + slice2.size(),
            DelimiterToSize(reinterpret_cast<const unsigned char *>(data)));

  // Back up
  stream.BackUp(5);

  // Test BytesAvailable() and ByteCount()
  EXPECT_EQ(slice1.size() + slice2.size() + 5, // +5 bytes for the delimiter
            stream.BytesAvailable());
  EXPECT_EQ(0, stream.ByteCount());

  // Test the slice again
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(5, size);
  EXPECT_EQ(slice1.size() + slice2.size(),
            DelimiterToSize(reinterpret_cast<const unsigned char *>(data)));

  // Test the first slice
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(slice1.size(), size);
  EXPECT_EQ(slice1, std::string(reinterpret_cast<const char *>(data), size));

  // Back up & test again
  stream.BackUp(size);
  EXPECT_EQ(slice1.size() + slice2.size(), stream.BytesAvailable());
  EXPECT_EQ(5, stream.ByteCount());
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(slice1.size(), size);
  EXPECT_EQ(slice1, std::string(reinterpret_cast<const char *>(data), size));

  // Now Back up 10 bytes & test again
  stream.BackUp(10);
  EXPECT_EQ(10 + slice2.size(), stream.BytesAvailable());
  EXPECT_EQ(5 + slice1.size() - 10, stream.ByteCount());
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(10, size);
  EXPECT_EQ(slice1.substr(slice1.size() - 10),
            std::string(reinterpret_cast<const char *>(data), size));

  // Test the second slice
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(slice2.size(), size);
  EXPECT_EQ(slice2, std::string(reinterpret_cast<const char *>(data), size));

  // Back up and test again
  stream.BackUp(size);
  EXPECT_EQ(slice2.size(), stream.BytesAvailable());
  EXPECT_EQ(5 + slice1.size(), stream.ByteCount());
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(slice2.size(), size);
  EXPECT_EQ(slice2, std::string(reinterpret_cast<const char *>(data), size));

  // Now Back up size - 1 bytes (all but 1) and check again
  stream.BackUp(size - 1);
  EXPECT_EQ(slice2.size() - 1, stream.BytesAvailable());
  EXPECT_EQ(5 + slice1.size() + 1, stream.ByteCount());
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(slice2.size() - 1, size);
  EXPECT_EQ(slice2.substr(1),
            std::string(reinterpret_cast<const char *>(data), size));

  // Check the end of the stream
  EXPECT_FALSE(stream.Next(&data, &size));
}

TEST_F(GrpcZeroCopyInputStreamTest, Skips) {
  GrpcZeroCopyInputStream stream;

  // Create and add a message
  std::string slice1 = "This is the first slice of the message.";
  std::string slice2 = "This is the second slice of the message.";
  std::string slice3 = "This is the third slice of the message.";
  std::string slice4 = "This is the fourth slice of the message.";
  const int kFrameSize = 5;

  stream.AddMessage(CreateByteBuffer(SliceData{slice1, slice2, slice3, slice4}),
                    true);
  stream.Finish();

  const void *data = nullptr;
  int size = -1;

  // Skip the message delimiter
  EXPECT_TRUE(stream.Skip(kFrameSize));

  // Test BytesAvailable() and ByteCount()
  EXPECT_EQ(slice1.size() + slice2.size() + slice3.size() + slice4.size(),
            stream.BytesAvailable());
  EXPECT_EQ(kFrameSize, stream.ByteCount());

  // Skip backwards (impossible -- no effect)
  EXPECT_FALSE(stream.Skip(-2));
  // Skip zero (no effect)
  EXPECT_TRUE(stream.Skip(0));

  // Test BytesAvailable() and ByteCount()
  EXPECT_EQ(slice1.size() + slice2.size() + slice3.size() + slice4.size(),
            stream.BytesAvailable());
  EXPECT_EQ(kFrameSize, stream.ByteCount());

  // Read the first slice
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(slice1.size(), size);
  EXPECT_EQ(slice1, std::string(reinterpret_cast<const char *>(data), size));

  // Back up to the start of slice1 & skip forward some bytes
  // Skip to the middle of slice1
  stream.BackUp(size);
  const int kSkip1 = slice1.size() - 10;
  EXPECT_TRUE(stream.Skip(kSkip1));

  EXPECT_EQ(slice1.size() - kSkip1 + slice2.size() + slice3.size() +
                slice4.size(),
            stream.BytesAvailable());
  EXPECT_EQ(kFrameSize + kSkip1, stream.ByteCount());

  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(slice1.size() - kSkip1, size);
  EXPECT_EQ(slice1.substr(kSkip1),
            std::string(reinterpret_cast<const char *>(data), size));

  // Back up some and skip even more, into slice
  stream.BackUp(10);
  const int kInSlice2 = 3;
  const int kSkip2 = 10 + kInSlice2;
  EXPECT_TRUE(stream.Skip(kSkip2));

  EXPECT_EQ(slice2.size() - kInSlice2 + slice3.size() + slice4.size(),
            stream.BytesAvailable());
  EXPECT_EQ(kFrameSize + slice1.size() + kInSlice2, stream.ByteCount());

  ASSERT_TRUE(stream.Next(&data, &size));
  ASSERT_EQ(slice2.size() - kInSlice2, size);
  EXPECT_EQ(slice2.substr(kInSlice2),
            std::string(reinterpret_cast<const char *>(data), size));

  // Back up 10 bytes in slice2 some and skip to end of slice2.
  stream.BackUp(10);
  ASSERT_TRUE(stream.Skip(10));
  EXPECT_EQ(slice3.size() + slice4.size(), stream.BytesAvailable());
  EXPECT_EQ(kFrameSize + slice1.size() + slice2.size(), stream.ByteCount());

  // Back up some and skip to last byte of second slice.
  stream.BackUp(3);
  EXPECT_TRUE(stream.Skip(2));
  EXPECT_EQ(1 + slice3.size() + slice4.size(), stream.BytesAvailable());
  EXPECT_EQ(kFrameSize + slice1.size() + slice2.size() - 1, stream.ByteCount());

  // Test the last byte of the second slice
  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(1, size);
  EXPECT_EQ(slice2.substr(slice2.size() - 1),
            std::string(reinterpret_cast<const char *>(data), size));

  // Back up and skip over all of slice 3 and then into slice 4
  stream.BackUp(10);
  EXPECT_TRUE(stream.Skip(10 + slice3.size() + 8));
  EXPECT_EQ(slice4.size() - 8, stream.BytesAvailable());
  EXPECT_EQ(kFrameSize + slice1.size() + slice2.size() + slice3.size() + 8,
            stream.ByteCount());

  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(slice4.size() - 8, size);
  EXPECT_EQ(slice4.substr(8),
            std::string(reinterpret_cast<const char *>(data), size));

  // Now Back up, skip exactly to end of stream and back up again
  stream.BackUp(4);
  EXPECT_TRUE(stream.Skip(4));
  stream.BackUp(4);
  EXPECT_EQ(4, stream.BytesAvailable());
  EXPECT_EQ(kFrameSize - 4 + slice1.size() + slice2.size() + slice3.size() +
                slice4.size(),
            stream.ByteCount());

  EXPECT_TRUE(stream.Next(&data, &size));
  EXPECT_EQ(4, size);
  EXPECT_EQ(slice4.substr(slice4.size() - 4),
            std::string(reinterpret_cast<const char *>(data), size));

  // Skip past the end of the stream
  EXPECT_FALSE(stream.Skip(100));
  EXPECT_EQ(0, stream.BytesAvailable());
  EXPECT_EQ(kFrameSize + slice1.size() + slice2.size() + slice3.size() +
                slice4.size(),
            stream.ByteCount());

  // Check the end of the stream
  EXPECT_FALSE(stream.Next(&data, &size));

  // Skip even some more
  EXPECT_FALSE(stream.Skip(100));
  EXPECT_EQ(0, stream.BytesAvailable());
  EXPECT_EQ(kFrameSize + slice1.size() + slice2.size() + slice3.size() +
                slice4.size(),
            stream.ByteCount());
}

TEST_F(GrpcZeroCopyInputStreamTest, NotOwnedMessages) {
  // Create and add a messages
  std::string slice1 = "000000000000000000000Message 1";
  std::string slice2 = "000000000000000000000Message 2";
  auto message1 = CreateByteBuffer(SliceData{1, slice1});
  auto message2 = CreateByteBuffer(SliceData{1, slice2});

  {
    GrpcZeroCopyInputStream stream;
    stream.AddMessage(message1, false);
    stream.AddMessage(message2, false);
    stream.Finish();

    // Test the message1 delimiter
    const void *data = nullptr;
    int size = -1;
    EXPECT_TRUE(stream.Next(&data, &size));
    EXPECT_EQ(5, size);
    EXPECT_EQ(slice1.size(),
              DelimiterToSize(reinterpret_cast<const unsigned char *>(data)));

    // Test slice1
    EXPECT_TRUE(stream.Next(&data, &size));
    EXPECT_EQ(slice1.size(), size);
    EXPECT_EQ(slice1, std::string(reinterpret_cast<const char *>(data), size));

    // Test the message2 delimiter
    EXPECT_TRUE(stream.Next(&data, &size));
    EXPECT_EQ(5, size);
    EXPECT_EQ(slice2.size(),
              DelimiterToSize(reinterpret_cast<const unsigned char *>(data)));

    // Test slice2
    EXPECT_TRUE(stream.Next(&data, &size));
    EXPECT_EQ(slice2.size(), size);
    EXPECT_EQ(slice2, std::string(reinterpret_cast<const char *>(data), size));

    // Test the end of the stream
    EXPECT_FALSE(stream.Next(&data, &size));
  }

  // Destroy the buffers as the stream hasn't.
  grpc_byte_buffer_destroy(message1);
  grpc_byte_buffer_destroy(message2);
}

} // namespace
} // namespace testing
} // namespace grpc
} // namespace api_manager
} // namespace google
