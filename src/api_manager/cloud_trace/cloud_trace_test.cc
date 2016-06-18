// Copyright (C) Endpoints Server Proxy Authors
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
#include "src/api_manager/cloud_trace/cloud_trace.h"

#include "google/devtools/cloudtrace/v1/trace.pb.h"
#include "gtest/gtest.h"
#include "src/api_manager/mock_api_manager_environment.h"

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

  std::unique_ptr<ApiManagerEnvInterface> env_;
  std::unique_ptr<auth::ServiceAccountToken> sa_token_;
};

TEST_F(CloudTraceTest, TestCloudTraceConfig) {
  CloudTraceConfig cloud_trace_config(sa_token_.get(),
                                      "http://fake.googleapis.com");
  ASSERT_EQ(cloud_trace_config.cloud_trace_address(),
            "http://fake.googleapis.com");
}

TEST_F(CloudTraceTest, TestCloudTrace) {
  CloudTrace cloud_trace;

  // After created, there should be a root span.
  ASSERT_EQ(cloud_trace.trace()->spans_size(), 1);
  ASSERT_EQ(cloud_trace.trace()->spans(0).name(), "API_MANAGER_ROOT");
  // End time should be empty now.
  ASSERT_EQ(cloud_trace.trace()->spans(0).end_time().DebugString(), "");

  google::devtools::cloudtrace::v1::TraceSpan *trace_span =
      cloud_trace.trace()->add_spans();
  trace_span->set_name("Span1");

  ASSERT_EQ(cloud_trace.trace()->spans_size(), 2);
  ASSERT_EQ(cloud_trace.trace()->spans(1).name(), "Span1");

  std::shared_ptr<CloudTraceSpan> cloud_trace_span(
      GetTraceSpan(&cloud_trace, "Span2"));
  TRACE(cloud_trace_span) << "Message";
  cloud_trace_span.reset();

  ASSERT_EQ(cloud_trace.trace()->spans_size(), 3);
  ASSERT_EQ(cloud_trace.trace()->spans(2).name(), "Span2");
  ASSERT_EQ(cloud_trace.trace()->spans(2).labels().size(), 1);
  ASSERT_EQ(cloud_trace.trace()->spans(2).labels().find("000")->second,
            "Message");

  cloud_trace.EndRootSpan();
  // After EndRootSpan, end time should not be empty.
  ASSERT_NE(cloud_trace.trace()->spans(0).end_time().DebugString(), "");
}

TEST_F(CloudTraceTest, TestCloudTraceSpanDisabled) {
  std::shared_ptr<CloudTraceSpan> cloud_trace_span(
      GetTraceSpan(nullptr, "Span"));
  // Ensure no core dump calling TRACE when cloud_trace_span is nullptr.
  TRACE(cloud_trace_span) << "Message";
  ASSERT_FALSE(cloud_trace_span);
}

}  // namespace

}  // cloud_trace
}  // namespace api_manager
}  // namespace google