/* Copyright (C) Extensible Service Proxy Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//
#ifndef API_MANAGER_CLOUD_TRACE_CLOUD_TRACE_H_
#define API_MANAGER_CLOUD_TRACE_CLOUD_TRACE_H_

#include <sstream>
#include <vector>

#include "google/devtools/cloudtrace/v1/trace.pb.h"
#include "google/protobuf/map.h"
#include "src/api_manager/cloud_trace/aggregator.h"
#include "src/api_manager/cloud_trace/sampler.h"

namespace google {
namespace api_manager {
namespace cloud_trace {

enum HeaderType {
  CLOUD_TRACE_CONTEXT = 1,
  GRPC_TRACE_CONTEXT = 2,
};

// Stores traces and metadata for one request. The instance of this class is
// put in request_context.
// When a CloudTrace instance is initialized, it creates a RootSpan called
// ESP_ROOT that will be a parent span of all other trace spans. Start time
// of this root span is recorded in constructor and end time is recorded when
// EndRootSpan is called.
class CloudTrace final {
 public:
  // Construct with give Trace proto object. This constructor must only be
  // called with non-null pointer.
  CloudTrace(google::devtools::cloudtrace::v1::Trace *trace,
             const std::string &options, HeaderType header);

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

  const std::string &options() const { return options_; }

  const HeaderType header_type() const { return header_type_; }

  std::string ToTraceContextHeader(uint64_t span_id) const;

 private:
  std::unique_ptr<google::devtools::cloudtrace::v1::Trace> trace_;
  google::devtools::cloudtrace::v1::TraceSpan *root_span_;
  std::string options_;
  std::string original_trace_context_;
  HeaderType header_type_;
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
 public:
  // Initializes a trace span whose parent is the api manager root.
  CloudTraceSpan(CloudTrace *cloud_trace, const std::string &span_name);

  // Initializes a trace span using the given trace span as parent.
  CloudTraceSpan(CloudTraceSpan *parent, const std::string &span_name);

  ~CloudTraceSpan();

  const google::devtools::cloudtrace::v1::TraceSpan *trace_span() const {
    return trace_span_;
  }

 private:
  friend class TraceStream;
  void Write(const std::string &msg);
  void InitWithParentSpanId(const std::string &span_name,
                            protobuf::uint64 parent_span_id);
  CloudTrace *cloud_trace_;
  google::devtools::cloudtrace::v1::TraceSpan *trace_span_;
  std::vector<std::string> messages_;
};

// Parses the trace_context and determines if cloud trace should
// be produced for the request. If so, creates an initialized CloudTrace object.
// Otherwise return nullptr.
// For trace_context, pass the value of "X-Cloud-Trace-Context" HTTP header.
CloudTrace *CreateCloudTrace(const std::string &trace_context,
                             const std::string &root_span_name,
                             HeaderType header_type,
                             Sampler *sampler = nullptr);

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
