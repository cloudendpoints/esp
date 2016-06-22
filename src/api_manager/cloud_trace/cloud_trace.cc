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
#include "cloud_trace.h"

#include <chrono>
#include <iomanip>
#include <random>
#include <string>
#include "google/protobuf/timestamp.pb.h"

using google::devtools::cloudtrace::v1::Trace;
using google::devtools::cloudtrace::v1::TraceSpan;
using google::devtools::cloudtrace::v1::TraceSpan_SpanKind;
using google::protobuf::Timestamp;

namespace google {
namespace api_manager {
namespace cloud_trace {
namespace {

const char kCloudTraceService[] = "/google.devtools.cloudtrace.v1.TraceService";

// Generate a random unsigned 64-bit integer.
uint64_t RandomUInt64();

// Generate a random unsigned 128-bit integer in hex string format.
std::string RandomUInt128HexString();

// Get the timestamp for now.
void GetNow(Timestamp *ts);
}  // namespace

CloudTraceConfig::CloudTraceConfig(auth::ServiceAccountToken *sa_token,
                                   std::string cloud_trace_address)
    : cloud_trace_address_(cloud_trace_address) {
  sa_token->SetAudience(auth::ServiceAccountToken::JWT_TOKEN_FOR_CLOUD_TRACING,
                        cloud_trace_address_ + kCloudTraceService);
}

CloudTrace::CloudTrace() : CloudTrace(std::string()) {}

CloudTrace::CloudTrace(std::string trace_context) {
  trace_.reset(new Trace);

  // TODO: parse trace_context and init from parent trace span.
  // Create root span.
  trace_->set_trace_id(RandomUInt128HexString());
  root_span_ = trace_->mutable_spans()->Add();
  root_span_->set_kind(TraceSpan_SpanKind::TraceSpan_SpanKind_RPC_SERVER);
  root_span_->set_span_id(RandomUInt64());
  root_span_->set_name("API_MANAGER_ROOT");
  GetNow(root_span_->mutable_start_time());
}

void CloudTrace::SetProjectId(const std::string &project_id) {
  trace_->set_project_id(project_id);
}

void CloudTrace::EndRootSpan() { GetNow(root_span_->mutable_end_time()); }

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
  GetNow(trace_span_->mutable_start_time());
}

CloudTraceSpan::~CloudTraceSpan() {
  if (!cloud_trace_) {
    // Trace is disabled.
    return;
  }
  GetNow(trace_span_->mutable_end_time());
  for (unsigned int i = 0; i < messages.size(); ++i) {
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(3) << i;
    std::string sequence = stream.str();
    trace_span_->mutable_labels()->insert({sequence, messages[i]});
  }
}

void CloudTraceSpan::Write(const std::string &msg) {
  if (!cloud_trace_) {
    // Trace is disabled.
    return;
  }
  messages.push_back(msg);
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

std::string RandomUInt128HexString() {
  std::stringstream stream;

  stream << std::setfill('0') << std::setw(sizeof(uint64_t) * 2) << std::hex
         << RandomUInt64();
  stream << std::setfill('0') << std::setw(sizeof(uint64_t) * 2) << std::hex
         << RandomUInt64();

  return stream.str();
}

void GetNow(Timestamp *ts) {
  long long nanos =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())
          .count();
  ts->set_seconds(nanos / 1000000000);
  ts->set_nanos(nanos % 1000000000);
}

}  // namespace
}  // cloud_trace
}  // api_manager
}  // google