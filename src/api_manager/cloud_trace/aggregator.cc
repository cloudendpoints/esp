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
#include "src/api_manager/cloud_trace/aggregator.h"

#include "src/api_manager/utils/marshalling.h"

using google::api_manager::utils::Status;
using google::devtools::cloudtrace::v1::Traces;
using google::devtools::cloudtrace::v1::Trace;

namespace google {
namespace api_manager {
namespace cloud_trace {
namespace {

const char kCloudTraceService[] = "/google.devtools.cloudtrace.v1.TraceService";

}  // namespace

Aggregator::Aggregator(auth::ServiceAccountToken *sa_token,
                       const std::string &cloud_trace_address,
                       int aggregate_time_millisec, int cache_max_size,
                       double minimum_qps, ApiManagerEnvInterface *env)
    : sa_token_(sa_token),
      cloud_trace_address_(cloud_trace_address),
      aggregate_time_millisec_(aggregate_time_millisec),
      cache_max_size_(cache_max_size),
      traces_(new Traces),
      env_(env),
      sampler_(minimum_qps) {
  sa_token_->SetAudience(auth::ServiceAccountToken::JWT_TOKEN_FOR_CLOUD_TRACING,
                         cloud_trace_address_ + kCloudTraceService);
}

void Aggregator::Init() {
  if (aggregate_time_millisec_ == 0) {
    return;
  }
  timer_ = env_->StartPeriodicTimer(
      std::chrono::milliseconds(aggregate_time_millisec_),
      [this]() { SendAndClearTraces(); });
}

Aggregator::~Aggregator() {
  if (timer_) {
    timer_->Stop();
  }
}

void Aggregator::SendAndClearTraces() {
  if (traces_->traces_size() == 0 || project_id_.empty()) {
    env_->LogDebug(
        "Not sending request to CloudTrace: no traces or "
        "project_id is empty.");
    traces_->clear_traces();
    return;
  }

  // Add project id into each trace object.
  for (int i = 0; i < traces_->traces_size(); ++i) {
    traces_->mutable_traces(i)->set_project_id(project_id_);
  }

  std::unique_ptr<HTTPRequest> http_request(new HTTPRequest(
      [this](Status status, std::map<std::string, std::string> &&,
             std::string &&body) {
        if (status.code() < 0) {
          env_->LogError("Trace Request Failed." + status.ToString());
        } else {
          env_->LogDebug("Trace Response: " + status.ToString() + "\n" + body);
        }
      }));

  std::string url =
      cloud_trace_address_ + "/v1/projects/" + project_id_ + "/traces";

  std::string request_body;

  ProtoToJson(*traces_, &request_body, utils::DEFAULT);
  traces_->clear_traces();
  env_->LogDebug("Sending request to Cloud Trace.");
  env_->LogDebug(request_body);

  http_request->set_url(url)
      .set_method("PATCH")
      .set_auth_token(sa_token_->GetAuthToken(
          auth::ServiceAccountToken::JWT_TOKEN_FOR_CLOUD_TRACING))
      .set_header("Content-Type", "application/json")
      .set_body(request_body);

  env_->RunHTTPRequest(std::move(http_request));
}

void Aggregator::AppendTrace(Trace *trace) {
  traces_->mutable_traces()->AddAllocated(trace);
  if (traces_->traces_size() > cache_max_size_) {
    SendAndClearTraces();
  }
}

}  // cloud_trace
}  // namespace api_manager
}  // namespace google
