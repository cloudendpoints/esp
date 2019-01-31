// Copyright (C) Extensible Service Proxy Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "src/api_manager/cloud_trace/cloud_trace.h"

#include "absl/strings/escaping.h"
#include "google/devtools/cloudtrace/v1/trace.pb.h"
#include "gtest/gtest.h"
#include "src/api_manager/mock_api_manager_environment.h"

using google::devtools::cloudtrace::v1::TraceSpan;

namespace google {
namespace api_manager {
namespace cloud_trace {
namespace {

class CloudTraceTest : public ::testing::Test {
 public:
  void SetUp() {
    env_.reset(new ::testing::NiceMock<MockApiManagerEnvironmentWithLog>());
    sa_token_ = std::unique_ptr<auth::ServiceAccountToken>(
        new auth::ServiceAccountToken(env_.get()));
  }

  std::string Base64Escape(absl::string_view str) {
    std::string ret;
    absl::Base64Escape(str, &ret);
    return ret;
  }

  std::unique_ptr<ApiManagerEnvInterface> env_;
  std::unique_ptr<auth::ServiceAccountToken> sa_token_;
};

TEST_F(CloudTraceTest, TestCloudTraceWithCloudHeader) {
  std::unique_ptr<CloudTrace> cloud_trace(
      CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=1", "root-span",
                       HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_TRUE(cloud_trace);

  // After created, there should be a root span.
  ASSERT_EQ(cloud_trace->trace()->spans_size(), 1);
  ASSERT_EQ(cloud_trace->trace()->spans(0).name(), "root-span");
  // End time should be empty now.
  ASSERT_EQ(cloud_trace->trace()->spans(0).end_time().DebugString(), "");

  TraceSpan *trace_span = cloud_trace->trace()->add_spans();
  trace_span->set_name("Span1");

  ASSERT_EQ(cloud_trace->trace()->spans_size(), 2);
  ASSERT_EQ(cloud_trace->trace()->spans(1).name(), "Span1");

  std::shared_ptr<CloudTraceSpan> cloud_trace_span(
      CreateSpan(cloud_trace.get(), "Span2"));
  TRACE(cloud_trace_span) << "Message";
  cloud_trace_span.reset();

  ASSERT_EQ(cloud_trace->trace()->spans_size(), 3);
  ASSERT_EQ(cloud_trace->trace()->spans(2).name(), "Span2");
  // the agent label and the "Message".
  ASSERT_EQ(cloud_trace->trace()->spans(2).labels().size(), 2);
  ASSERT_EQ(cloud_trace->trace()->spans(2).labels().find("000")->second,
            "Message");

  cloud_trace->EndRootSpan();
  // After EndRootSpan, end time should not be empty.
  ASSERT_NE(cloud_trace->trace()->spans(0).end_time().DebugString(), "");
}

TEST_F(CloudTraceTest, TestCloudTraceSpanDisabled) {
  std::shared_ptr<CloudTraceSpan> cloud_trace_span(CreateSpan(nullptr, "Span"));
  // Ensure no core dump calling TRACE when cloud_trace_span is nullptr.
  TRACE(cloud_trace_span) << "Message";
  ASSERT_FALSE(cloud_trace_span);
}

TEST_F(CloudTraceTest, TestParseCloudTraceContextHeader) {
  // Disabled if empty.
  ASSERT_EQ(nullptr, CreateCloudTrace("", "", HeaderType::CLOUD_TRACE_CONTEXT));
  // Disabled for malformed prefix
  ASSERT_EQ(nullptr,
            CreateCloudTrace("o=1", "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr,
            CreateCloudTrace("o=", "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr,
            CreateCloudTrace("o=1foo", "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr,
            CreateCloudTrace("o=foo1", "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace("o=113471230948140", "",
                                      HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace("o=1;foo=bar", "",
                                      HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr,
            CreateCloudTrace(";o=", "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr,
            CreateCloudTrace(";o=1", "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr,
            CreateCloudTrace(";o=1foo", "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr,
            CreateCloudTrace(";o=foo1", "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace(";o=113471230948140", "",
                                      HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace(";o=1;foo=bar", "",
                                      HeaderType::CLOUD_TRACE_CONTEXT));
  // Disabled if trace id length < 32
  ASSERT_EQ(nullptr,
            CreateCloudTrace("123;o=1", "", HeaderType::CLOUD_TRACE_CONTEXT));
  // Disabled if trace id is not hex string ('q' in last position)
  ASSERT_EQ(nullptr, CreateCloudTrace("e133eacd437d8a12068fd902af3962dq;o=1",
                                      "", HeaderType::CLOUD_TRACE_CONTEXT));
  // Disabled if no option invalid or not provided.
  ASSERT_EQ(nullptr, CreateCloudTrace("e133eacd437d8a12068fd902af3962d8", "",
                                      HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;", "",
                                      HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=", "",
                                      HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=4",
                                      "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=-1",
                                      "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr,
            CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=12345", "",
                             HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=1foo",
                                      "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=foo1",
                                      "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace(
                         "e133eacd437d8a12068fd902af3962d8;o=113471230948140",
                         "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace("e133eacd437d8a12068fd902af3962d8", "",
                                      HeaderType::CLOUD_TRACE_CONTEXT));
  // Disabled if option explicitly says so. Note: first bit of number "o"
  // indicated whether trace is enabled.
  ASSERT_EQ(nullptr, CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=0",
                                      "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr, CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=2",
                                      "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr,
            CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=0;o=1", "",
                             HeaderType::CLOUD_TRACE_CONTEXT));
  // Disabled if span id is illegal
  ASSERT_EQ(nullptr, CreateCloudTrace("e133eacd437d8a12068fd902af3962d8/xx;o=1",
                                      "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr,
            CreateCloudTrace("e133eacd437d8a12068fd902af3962d8/1xx;o=1", "",
                             HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr,
            CreateCloudTrace("e133eacd437d8a12068fd902af3962d8/xx1;o=1", "",
                             HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(nullptr,
            CreateCloudTrace(
                "e133eacd437d8a12068fd902af3962d8/18446744073709551616;o=1", "",
                HeaderType::CLOUD_TRACE_CONTEXT));

  std::unique_ptr<CloudTrace> cloud_trace;

  // parent trace id should be 0(default) if span id is not provided.
  cloud_trace.reset(CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=1", "",
                                     HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_TRUE(cloud_trace);
  ASSERT_EQ(0, cloud_trace->root_span()->parent_span_id());
  ASSERT_EQ(1, cloud_trace->trace()->spans_size());
  ASSERT_EQ("o=1", cloud_trace->options());

  // Should also be enabled for "o=3"
  cloud_trace.reset(CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=3", "",
                                     HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_TRUE(cloud_trace);
  ASSERT_EQ("o=3", cloud_trace->options());

  cloud_trace.reset(CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=1;",
                                     "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_TRUE(cloud_trace);
  ASSERT_EQ("o=1;", cloud_trace->options());

  cloud_trace.reset(CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=1;o=0",
                                     "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_TRUE(cloud_trace);
  ASSERT_EQ("o=1;o=0", cloud_trace->options());

  // Verify capital hex digits should pass
  cloud_trace.reset(CreateCloudTrace("46F1ADB8573CC0F3C4156B5EA7E0E3DC;o=1", "",
                                     HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_TRUE(cloud_trace);

  // Parent trace id should be set if span id is provided.
  cloud_trace.reset(
      CreateCloudTrace("e133eacd437d8a12068fd902af3962d8/12345;o=1", "",
                       HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_TRUE(cloud_trace);
  ASSERT_EQ(12345, cloud_trace->root_span()->parent_span_id());
  // Parent trace id is max uint64
  cloud_trace.reset(CreateCloudTrace(
      "e133eacd437d8a12068fd902af3962d8/18446744073709551615;o=1", "",
      HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_TRUE(cloud_trace);
  ASSERT_EQ(18446744073709551615U, cloud_trace->root_span()->parent_span_id());

  // Should not crash if unrecognized option is provided.
  cloud_trace.reset(
      CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;foo=bar;o=1", "",
                       HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_TRUE(cloud_trace);
  ASSERT_EQ("foo=bar;o=1", cloud_trace->options());

  cloud_trace.reset(CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;x;o=1",
                                     "", HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_TRUE(cloud_trace);
  ASSERT_EQ("x;o=1", cloud_trace->options());

  cloud_trace.reset(
      CreateCloudTrace("e133eacd437d8a12068fd902af3962d8;o=1;foo=bar", "",
                       HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_TRUE(cloud_trace);
  ASSERT_EQ("o=1;foo=bar", cloud_trace->options());
}

TEST_F(CloudTraceTest, TestFormatCloudTraceContextHeader) {
  std::unique_ptr<CloudTrace> cloud_trace(
      CreateCloudTrace("e133eacd437d8a12068fd902af3962d8/12345;o=1", "",
                       HeaderType::CLOUD_TRACE_CONTEXT));
  ASSERT_EQ(cloud_trace->ToTraceContextHeader(12345),
            "e133eacd437d8a12068fd902af3962d8/12345;o=1");
  ASSERT_EQ(cloud_trace->ToTraceContextHeader(12),
            "e133eacd437d8a12068fd902af3962d8/12;o=1");
  ASSERT_EQ(cloud_trace->ToTraceContextHeader(18446744073709551615U),
            "e133eacd437d8a12068fd902af3962d8/18446744073709551615;o=1");
}

TEST_F(CloudTraceTest, TestParseGrpcTraceContextHeader) {
  std::unique_ptr<CloudTrace> cloud_trace;
  {
    // Trace options missing.
    constexpr char header[] = {
        0,                                               // version_id
        0,                                               // trace_id field
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  // hi
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  // lo
        1,                                               // span_id field
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  // span_id
        2,                                               // options missing
    };
    ASSERT_EQ(nullptr,
              CreateCloudTrace(
                  Base64Escape(absl::string_view(header, sizeof(header))),
                  "root-span", HeaderType::GRPC_TRACE_CONTEXT));
  }

  {
    // Unrecognized version_id.
    constexpr char header[] = {
        123,                                             // version_id not 0
        0,                                               // trace_id field
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  // hi
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  // lo
        1,                                               // span_id field
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  // span_id
        2,                                               // trace_options field
        1,                                               // tracing enabled
    };
    ASSERT_EQ(nullptr,
              CreateCloudTrace(
                  Base64Escape(absl::string_view(header, sizeof(header))),
                  "root-span", HeaderType::GRPC_TRACE_CONTEXT));
  }

  {
    // Invalid trace id 0.
    constexpr char header[] = {
        0,                                               // version_id
        0,                                               // trace_id field
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // hi
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // lo
        1,                                               // span_id field
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  // span_id
        2,                                               // trace_options field
        1,                                               // tracing enabled
    };
    ASSERT_EQ(nullptr,
              CreateCloudTrace(
                  Base64Escape(absl::string_view(header, sizeof(header))),
                  "root-span", HeaderType::GRPC_TRACE_CONTEXT));
  }

  {
    // TraceId short.
    constexpr char header[] = {
        0,                                               // version_id
        0,                                               // trace_id field
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  // hi
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,        // lo
        1,                                               // span_id field
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  // span_id
        2,                                               // trace_options field
        1,                                               // tracing enabled
    };
    ASSERT_EQ(nullptr,
              CreateCloudTrace(
                  Base64Escape(absl::string_view(header, sizeof(header))),
                  "root-span", HeaderType::GRPC_TRACE_CONTEXT));
  }

  {
    // SpanId short.
    constexpr char header[] = {
        0,                                               // version_id
        0,                                               // trace_id field
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  // hi
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  // lo
        1,                                               // span_id field
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,        // span_id
        2,                                               // trace_options field
        1,                                               // tracing enabled
    };
    ASSERT_EQ(nullptr,
              CreateCloudTrace(
                  Base64Escape(absl::string_view(header, sizeof(header))),
                  "root-span", HeaderType::GRPC_TRACE_CONTEXT));
  }

  // Header not base64 encoded.
  {
    constexpr char header[] = {
        0,                                               // version
        0,                                               // trace_id field
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  // hi
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  // lo
        1,                                               // span_id field
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x39,  // span_id 12345
        2,                                               // trace_options field
        1,                                               // options: enabled
    };
    ASSERT_EQ(nullptr,
              CreateCloudTrace(std::string(header, sizeof(header)), "root-span",
                               HeaderType::GRPC_TRACE_CONTEXT));
  }

  // Parent trace id should be set if span id is provided.
  {
    constexpr char header[] = {
        0,                                               // version
        0,                                               // trace_id field
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  // hi
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  // lo
        1,                                               // span_id field
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x39,  // span_id 12345
        2,                                               // trace_options field
        1,                                               // options: enabled
    };
    cloud_trace.reset(CreateCloudTrace(
        Base64Escape(absl::string_view(header, sizeof(header))), "root-span",
        HeaderType::GRPC_TRACE_CONTEXT));
    ASSERT_TRUE(cloud_trace);
    ASSERT_EQ(cloud_trace->trace()->trace_id(),
              "10111213141516172021222324252627");
    ASSERT_EQ(cloud_trace->trace()->spans_size(), 1);
    ASSERT_EQ(cloud_trace->trace()->spans(0).parent_span_id(), 12345);
    ASSERT_EQ("o=1", cloud_trace->options());
  }

  // SpanId 0 means no parent
  {
    constexpr char header[] = {
        0,                                               // version_id
        0,                                               // trace_id field
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  // hi
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  // lo
        1,                                               // span_id field
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // span_id
        2,                                               // trace_options field
        1,                                               // tracing enabled
    };
    cloud_trace.reset(CreateCloudTrace(
        Base64Escape(absl::string_view(header, sizeof(header))), "root-span",
        HeaderType::GRPC_TRACE_CONTEXT));
    ASSERT_TRUE(cloud_trace);
    ASSERT_EQ(cloud_trace->trace()->trace_id(),
              "10111213141516172021222324252627");
    ASSERT_EQ(cloud_trace->trace()->spans_size(), 1);
    ASSERT_EQ(cloud_trace->trace()->spans(0).parent_span_id(), 0);
    ASSERT_EQ("o=1", cloud_trace->options());
  }
}

TEST_F(CloudTraceTest, TestFormatGrpcTraceContextHeader) {
  constexpr char header[] = {
      0,                                               // version
      0,                                               // trace_id field
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  // hi
      0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  // lo
      1,                                               // span_id field
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x39,  // span_id 12345
      2,                                               // trace_options field
      1,                                               // options: enabled
  };
  std::unique_ptr<CloudTrace> cloud_trace(
      CreateCloudTrace(Base64Escape(absl::string_view(header, sizeof(header))),
                       "root-span", HeaderType::GRPC_TRACE_CONTEXT));
  ASSERT_TRUE(cloud_trace);
  ASSERT_EQ(cloud_trace->ToTraceContextHeader(12345),
            Base64Escape(absl::string_view(header, sizeof(header))));
  constexpr char expected_header1[] = {
      0,  // version
      0,  // trace_id field
      0x10, 0x11, 0x12, 0x13,
      0x14, 0x15, 0x16, 0x17,  // hi
      0x20, 0x21, 0x22, 0x23,
      0x24, 0x25, 0x26, 0x27,  // lo
      1,                       // span_id field
      0x30, 0x31, 0x32, 0x33,
      0x34, 0x35, 0x36, 0x37,  // span_id 0x3031323334353637
      2,                       // trace_options field
      1,                       // options: enabled
  };
  ASSERT_EQ(cloud_trace->ToTraceContextHeader(0x3031323334353637),
            Base64Escape(
                absl::string_view(expected_header1, sizeof(expected_header1))));
  constexpr char expected_header2[] = {
      0,                                               // version
      0,                                               // trace_id field
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  // hi
      0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  // lo
      1,                                               // span_id field
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x39,  // span_id 57
      2,                                               // trace_options field
      1,                                               // options: enabled
  };
  ASSERT_EQ(cloud_trace->ToTraceContextHeader(57),
            Base64Escape(
                absl::string_view(expected_header2, sizeof(expected_header2))));
}

}  // namespace
}  // cloud_trace
}  // namespace api_manager
}  // namespace google
