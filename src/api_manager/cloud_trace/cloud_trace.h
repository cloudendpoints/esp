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
#ifndef API_MANAGER_CLOUD_TRACE_CLOUD_TRACE_H_
#define API_MANAGER_CLOUD_TRACE_CLOUD_TRACE_H_

#include <sstream>
#include <vector>

#include "google/devtools/cloudtrace/v1/trace.pb.h"
#include "google/protobuf/map.h"
#include "src/api_manager/auth/service_account_token.h"

namespace google {
namespace api_manager {
namespace cloud_trace {

// TODO: simplify class naming in this file.
// Stores cloud trace configurations shared within the job. There should be
// only one such instance. The instance is put in service_context.
class CloudTraceConfig {
 public:
  CloudTraceConfig(auth::ServiceAccountToken *sa_token,
                   std::string cloud_trace_address);

  const std::string &cloud_trace_address() const {
    return cloud_trace_address_;
  }

 private:
  std::string cloud_trace_address_;
};

// Stores traces and metadata for one request. The instance of this class is
// put in request_context.
// When a CloudTrace instance is initialized, it creates a RootSpan called
// ESP_ROOT that will be a parent span of all other trace spans. Start time
// of this root span is recorded in constructor and end time is recorded when
// EndRootSpan is called.
class CloudTrace final {
 public:
  CloudTrace();

  // Construct with context header.
  CloudTrace(std::string trace_context);

  void SetProjectId(const std::string &project_id);

  void EndRootSpan();

  google::devtools::cloudtrace::v1::TraceSpan *root_span() {
    return root_span_;
  }

  google::devtools::cloudtrace::v1::Trace *trace() { return trace_.get(); }

  // Releases ownership of all traces and returns it.
  google::devtools::cloudtrace::v1::Trace *ReleaseTrace() {
    return trace_.release();
  }

 private:
  std::unique_ptr<google::devtools::cloudtrace::v1::Trace> trace_;
  google::devtools::cloudtrace::v1::TraceSpan *root_span_;
};

// This class stores messages written to a single trace span. There can be
// multiple trace spans for one request. Typically an instance of this class is
// initialized at the beginning of a function that needs to be traced.
//
// When an instance of CloudTraceSpan is destructed, its messages are written
// to the corresponding CloudTrace.
//
// Start time and end time of the trace span is recorded in constructor and
// destructor.
//
// Example of initializing a trace span:
// std::shared_ptr<CloudTraceSpan> trace_span(GetTraceSpan(cloud_trace,
//                                                         "MyFunc"));
//
class CloudTraceSpan {
  friend class TraceStream;

 public:
  // Initializes a trace span whose parent is the api manager root.
  CloudTraceSpan(CloudTrace *cloud_trace, const std::string &span_name);

  // Initializes a trace span using the given trace span as parent.
  CloudTraceSpan(CloudTraceSpan *parent, const std::string &span_name);

  ~CloudTraceSpan();

 private:
  void Write(const std::string &msg);
  void InitWithParentSpanId(const std::string &span_name,
                            protobuf::uint64 parent_span_id);
  CloudTrace *cloud_trace_;
  google::devtools::cloudtrace::v1::TraceSpan *trace_span_;
  std::vector<std::string> messages;
};

// Creates trace span if trace is enabled.
// Returns nullptr when cloud_trace is nullptr.
CloudTraceSpan *CreateSpan(CloudTrace *cloud_trace, const std::string &name);

// Creates a child trace span with the given parent span.
// Returns nullptr if parent is nullptr.
CloudTraceSpan *CreateChildSpan(CloudTraceSpan *parent,
                                const std::string &name);

// A helper class to create a stream-like write traces interface.
//
class TraceStream {
 public:
  TraceStream(std::shared_ptr<CloudTraceSpan> trace_span)
      : trace_span_(trace_span.get()){};

  ~TraceStream();

  template <class T>
  TraceStream &operator<<(T const &value) {
    info_ << value;
    return *this;
  }

 private:
  CloudTraceSpan *trace_span_;
  std::ostringstream info_;
};

// This class is used to explicitly ignore values in the conditional
// tracing macro TRACE_ENABLED, i.e. when trace is disabled this class becomes
// a no-op prefix.
class TraceMessageVoidify {
 public:
  TraceMessageVoidify() {}
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(const TraceStream &trace_out) {}
};

}  // cloud_trace
}  // api_manager
}  // google

// A helper macro to make tracing conditional.
#define TRACE_ENABLED(trace_span) \
  !(trace_span) ? (void)0         \
                : ::google::api_manager::cloud_trace::TraceMessageVoidify() &

// The macro to write traces. Example usage:
// TRACE(trace_span) << "Some message: " << some_str;
#define TRACE(trace_span)   \
  TRACE_ENABLED(trace_span) \
  ::google::api_manager::cloud_trace::TraceStream((trace_span))

#endif  // API_MANAGER_CLOUD_TRACE_CLOUD_TRACE_H_
