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

#include <cctype>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include "absl/base/internal/endian.h"
#include "absl/strings/escaping.h"
#include "google/protobuf/timestamp.pb.h"
#include "include/api_manager/utils/status.h"
#include "include/api_manager/utils/version.h"

using google::devtools::cloudtrace::v1::Trace;
using google::devtools::cloudtrace::v1::TraceSpan;
using google::devtools::cloudtrace::v1::TraceSpan_SpanKind;
using google::protobuf::Timestamp;

namespace google {
namespace api_manager {
namespace cloud_trace {
namespace {

// Cloud Trace agent label key
const char kCloudTraceAgentKey[] = "trace.cloud.google.com/agent";
// Cloud Trace agent label value
const char kServiceAgentPrefix[] = "esp/";
// Default trace options
const char kDefaultTraceOptions[] = "o=1";

// gRPC trace context constants
constexpr size_t kTraceIdFieldIdPos = 1;
constexpr size_t kSpanIdFieldIdPos = kTraceIdFieldIdPos + 16 + 1;
constexpr size_t kTraceOptionsFieldIdPos = kSpanIdFieldIdPos + 8 + 1;
constexpr size_t kGrpcTraceBinLen = 29;

// Generate a random unsigned 64-bit integer.
uint64_t RandomUInt64();

// Get a random string of 128 bit hex number
std::string RandomUInt128HexString();

// Get the timestamp for now.
void GetNow(Timestamp *ts);

// Get a new Trace object stored in the trace parameter. The new object has
// the given trace id and contains a root span with default settings.
void GetNewTrace(std::string trace_id_str, const std::string &root_span_name,
                 Trace **trace);

// Parse the cloud trace context header.
// Assigns Trace object to the trace pointer if context is parsed correctly and
// trace is enabled. Otherwise the pointer is not modified.
// If trace is enabled, the option will be modified to the one passed in.
//
// Grammar of the context header:
// trace-id  [“/” span-id] [ “;” “o” “=” trace_options ]
//
// trace-id      := hex representation of a 128 bit value
// span-id       := decimal representation of a 64 bit value
// trace-options := decimal representation of a 32 bit value
void GetTraceFromCloudTraceContextHeader(const std::string &trace_context,
                                         const std::string &root_span_name,
                                         Trace **trace, std::string *options);

// Parse the grpc trace context header.
// Assigns Trace object to the trace pointer if context is parsed correctly and
// trace is enabled. Otherwise the pointer is not modified.
// If trace is enabled, the option will be modified to the one passed in.
void GetTraceFromGRpcTraceContextHeader(const std::string &raw_trace_context,
                                        const std::string &root_span_name,
                                        Trace **trace, std::string *options);
}  // namespace

CloudTrace::CloudTrace(Trace *trace, const std::string &options,
                       HeaderType header_type)
    : trace_(trace), options_(options), header_type_(header_type) {
  // Root span must exist and must be the only span as of now.
  root_span_ = trace_->mutable_spans(0);
}

void CloudTrace::SetProjectId(const std::string &project_id) {
  trace_->set_project_id(project_id);
}

void CloudTrace::EndRootSpan() { GetNow(root_span_->mutable_end_time()); }

std::string CloudTrace::ToTraceContextHeader(uint64_t span_id) const {
  if (header_type_ == HeaderType::CLOUD_TRACE_CONTEXT) {
    std::ostringstream trace_context_stream;
    trace_context_stream << trace_->trace_id() << "/" << span_id << ";"
                         << options_;
    return trace_context_stream.str();
  } else {
    char tc[kGrpcTraceBinLen];
    // Version
    tc[0] = 0;
    // TraceId
    tc[kTraceIdFieldIdPos] = 0;
    std::string bytes_tid = absl::HexStringToBytes(trace_->trace_id());
    memcpy(tc + kTraceIdFieldIdPos + 1, bytes_tid.data(), 2 * sizeof(uint64_t));
    // SpanId
    tc[kSpanIdFieldIdPos] = 1;
    uint64_t sid = __builtin_bswap64(span_id);
    memcpy(tc + kSpanIdFieldIdPos + 1, (const char *)&sid, sizeof(uint64_t));
    // TraceOptions
    tc[kTraceOptionsFieldIdPos] = 2;
    tc[kTraceOptionsFieldIdPos + 1] = options_ == kDefaultTraceOptions ? 1 : 0;
    std::string trace_context;
    // For grpc the header must be base64 encoded because this is a binary
    // header.
    absl::Base64Escape(absl::string_view(tc, kGrpcTraceBinLen), &trace_context);
    return trace_context;
  }
}

CloudTraceSpan::CloudTraceSpan(CloudTrace *cloud_trace,
                               const std::string &span_name)
    : cloud_trace_(cloud_trace) {
  InitWithParentSpanId(span_name, cloud_trace_->root_span()->span_id());
}

CloudTraceSpan::CloudTraceSpan(CloudTraceSpan *parent,
                               const std::string &span_name)
    : cloud_trace_(parent->cloud_trace_) {
  InitWithParentSpanId(span_name, parent->trace_span_->span_id());
}

void CloudTraceSpan::InitWithParentSpanId(const std::string &span_name,
                                          protobuf::uint64 parent_span_id) {
  // TODO: this if is not needed, and probably the following two as well.
  // Fully test and remove them.
  if (!cloud_trace_) {
    // Trace is disabled.
    return;
  }
  trace_span_ = cloud_trace_->trace()->add_spans();
  trace_span_->set_kind(TraceSpan_SpanKind::TraceSpan_SpanKind_RPC_SERVER);
  trace_span_->set_span_id(RandomUInt64());
  trace_span_->set_parent_span_id(parent_span_id);
  trace_span_->set_name(span_name);
  // Agent label is defined as "<agent>/<version>".
  trace_span_->mutable_labels()->insert(
      {kCloudTraceAgentKey,
       kServiceAgentPrefix + utils::Version::instance().get()});
  GetNow(trace_span_->mutable_start_time());
}

CloudTraceSpan::~CloudTraceSpan() {
  if (!cloud_trace_) {
    // Trace is disabled.
    return;
  }
  GetNow(trace_span_->mutable_end_time());
  for (unsigned int i = 0; i < messages_.size(); ++i) {
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(3) << i;
    std::string sequence = stream.str();
    trace_span_->mutable_labels()->insert({sequence, messages_[i]});
  }
}

void CloudTraceSpan::Write(const std::string &msg) {
  if (!cloud_trace_) {
    // Trace is disabled.
    return;
  }
  messages_.push_back(msg);
}

CloudTrace *CreateCloudTrace(const std::string &trace_context,
                             const std::string &root_span_name,
                             HeaderType header_type, Sampler *sampler) {
  Trace *trace = nullptr;
  std::string options;
  switch (header_type) {
    case HeaderType::CLOUD_TRACE_CONTEXT:
      GetTraceFromCloudTraceContextHeader(trace_context, root_span_name, &trace,
                                          &options);
      break;
    case HeaderType::GRPC_TRACE_CONTEXT:
      GetTraceFromGRpcTraceContextHeader(trace_context, root_span_name, &trace,
                                         &options);
      break;
  }
  if (trace) {
    // When trace is triggered by the context header, refresh the previous
    // timestamp in sampler.
    if (sampler) {
      sampler->Refresh();
    }
    return new CloudTrace(trace, options, header_type);
  } else if (sampler && sampler->On()) {
    // Trace is turned on by sampler.
    GetNewTrace(RandomUInt128HexString(), root_span_name, &trace);
    return new CloudTrace(trace, kDefaultTraceOptions, header_type);
  } else {
    return nullptr;
  }
}

CloudTraceSpan *CreateSpan(CloudTrace *cloud_trace, const std::string &name) {
  if (cloud_trace != nullptr) {
    return new CloudTraceSpan(cloud_trace, name);
  } else {
    return nullptr;
  }
}

CloudTraceSpan *CreateChildSpan(CloudTraceSpan *parent,
                                const std::string &name) {
  if (parent != nullptr) {
    return new CloudTraceSpan(parent, name);
  } else {
    return nullptr;
  }
}

TraceStream::~TraceStream() { trace_span_->Write(info_.str()); }

namespace {
// TODO: this method is duplicated with a similar method in
// api_manager/context/request_context.cc. Consider merge them by moving to a
// common position.
uint64_t RandomUInt64() {
  static std::random_device random_device;
  static std::mt19937 generator(random_device());
  static std::uniform_int_distribution<uint64_t> distribution;
  return distribution(generator);
}

std::string HexUInt128(uint64_t hi, uint64_t lo) {
  std::stringstream stream;

  stream << std::setfill('0') << std::setw(sizeof(uint64_t) * 2) << std::hex
         << hi;
  stream << std::setfill('0') << std::setw(sizeof(uint64_t) * 2) << std::hex
         << lo;

  return stream.str();
}

std::string RandomUInt128HexString() {
  return HexUInt128(RandomUInt64(), RandomUInt64());
}

void GetNow(Timestamp *ts) {
  long long nanos =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())
          .count();
  ts->set_seconds(nanos / 1000000000);
  ts->set_nanos(nanos % 1000000000);
}

void GetNewTrace(std::string trace_id_str, const std::string &root_span_name,
                 Trace **trace) {
  *trace = new Trace;
  (*trace)->set_trace_id(trace_id_str);
  TraceSpan *root_span = (*trace)->add_spans();
  root_span->set_kind(TraceSpan_SpanKind::TraceSpan_SpanKind_RPC_SERVER);
  root_span->set_span_id(RandomUInt64());
  root_span->set_name(root_span_name);
  // Agent label is defined as "<agent>/<version>".
  root_span->mutable_labels()->insert(
      {kCloudTraceAgentKey,
       kServiceAgentPrefix + utils::Version::instance().get()});
  GetNow(root_span->mutable_start_time());
}

void GetTraceFromGRpcTraceContextHeader(const std::string &raw_trace_context,
                                        const std::string &root_span_name,
                                        Trace **trace, std::string *options) {
  std::string trace_context;
  // Grpc binary headers are base64 encoded, decode the header before parsing
  // it.
  if (!absl::Base64Unescape(raw_trace_context, &trace_context)) {
    // Not a valid base64 encoded string.
    return;
  }
  if (trace_context.length() != kGrpcTraceBinLen || trace_context[0] != 0) {
    // Size or version unknown.
    return;
  }

  if (trace_context[kTraceIdFieldIdPos] != 0 ||
      trace_context[kSpanIdFieldIdPos] != 1 ||
      trace_context[kTraceOptionsFieldIdPos] != 2) {
    // Field ids are not in the right positions.
    return;
  }

  if (!(trace_context[kTraceOptionsFieldIdPos + 1] & 1)) {
    // Trace is not enabled
    return;
  }

  *options = kDefaultTraceOptions;

  // Check for a valid trace id
  absl::string_view trace_id_str(trace_context.data() + kTraceIdFieldIdPos + 1,
                                 2 * sizeof(uint64_t));
  bool valid_trace_id = false;
  for (size_t i = 0; i < 2 * sizeof(uint64_t); i++) {
    if (trace_id_str[i] != 0) {
      valid_trace_id = true;
      break;
    }
  }
  if (!valid_trace_id) {
    // Invalid trace id
    return;
  }

  uint64_t span_id =
      absl::big_endian::Load64(trace_context.data() + kSpanIdFieldIdPos + 1);

  // At this point, trace is enabled and trace id is successfully parsed.
  GetNewTrace(absl::BytesToHexString(trace_id_str), root_span_name, trace);
  TraceSpan *root_span = (*trace)->mutable_spans(0);
  // Set parent of root span to the given one if provided.
  if (span_id != 0) {
    root_span->set_parent_span_id(span_id);
  }
}

void GetTraceFromCloudTraceContextHeader(const std::string &trace_context,
                                         const std::string &root_span_name,
                                         Trace **trace, std::string *options) {
  std::stringstream header_stream(trace_context);

  std::string trace_and_span_id;
  if (!getline(header_stream, trace_and_span_id, ';')) {
    // When trace_context is empty;
    return;
  }

  bool trace_enabled = false;
  std::string item;
  while (getline(header_stream, item, ';')) {
    if (item.substr(0, 2) == "o=") {
      int value;
      std::stringstream option_stream(item.substr(2));
      if ((option_stream >> value).fail() || !option_stream.eof()) {
        return;
      }
      if (value < 0 || value > 0b11) {
        // invalid option value.
        return;
      }
      *options = trace_context.substr(trace_context.find_first_of(';') + 1);
      // First bit indicates whether trace is enabled.
      if (!(value & 1)) {
        return;
      }
      // Trace is enabled, we can stop parsing the header.
      trace_enabled = true;
      break;
    }
  }
  if (!trace_enabled) {
    return;
  }

  // Parse trace_id/span_id
  std::string trace_id_str, span_id_str;
  size_t slash_pos = trace_and_span_id.find_first_of('/');
  if (slash_pos == std::string::npos) {
    trace_id_str = trace_and_span_id;
  } else {
    trace_id_str = trace_and_span_id.substr(0, slash_pos);
    span_id_str = trace_and_span_id.substr(slash_pos + 1);
  }

  // Trace id should be a 128-bit hex number (32 hex digits).
  if (trace_id_str.size() != 32) {
    return;
  }
  for (size_t i = 0; i < trace_id_str.size(); ++i) {
    if (!isxdigit(trace_id_str[i])) {
      return;
    }
  }

  uint64_t span_id = 0;
  // Parse the span id, disable trace when span id is illegal.
  if (!span_id_str.empty()) {
    std::stringstream span_id_stream(span_id_str);
    if ((span_id_stream >> span_id).fail() || !span_id_stream.eof()) {
      return;
    }
  }

  // At this point, trace is enabled and trace id is successfully parsed.
  GetNewTrace(trace_id_str, root_span_name, trace);
  TraceSpan *root_span = (*trace)->mutable_spans(0);
  // Set parent of root span to the given one if provided.
  if (span_id != 0) {
    root_span->set_parent_span_id(span_id);
  }
}

}  // namespace
}  // namespace cloud_trace
}  // namespace api_manager
}  // namespace google
