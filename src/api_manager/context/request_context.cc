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

#include "src/api_manager/context/request_context.h"

#include <uuid/uuid.h>
#include <sstream>

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {
namespace context {

namespace {

// Cloud Trace Context Header
const char kCloudTraceContextHeader[] = "X-Cloud-Trace-Context";

// Log message prefix for a success method.
const char kMessage[] = "Method: ";
// Log message prefix for an ignored method.
const char kIgnoredMessage[] =
    "Endpoints management skipped for an unrecognized HTTP call: ";
// Unknown HTTP verb.
const char kUnknownHttpVerb[] = "<Unknown HTTP Verb>";

// Service control does not currently support logging with an empty
// operation name so we use this value until fix is available.
const char kUnrecognizedOperation[] = "<Unknown Operation Name>";

// Maximum 36 byte string for UUID
const int kMaxUUIDBufSize = 40;

// Default api key names
const char kDefaultApiKeyQueryName1[] = "key";
const char kDefaultApiKeyQueryName2[] = "api_key";
const char kDefaultApiKeyHeaderName[] = "x-api-key";

// Default location
const char kDefaultLocation[] = "us-central1";

// Genereates a UUID string
std::string GenerateUUID() {
  char uuid_buf[kMaxUUIDBufSize];
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse(uuid, uuid_buf);
  return uuid_buf;
}

}  // namespace

using context::ServiceContext;

RequestContext::RequestContext(std::shared_ptr<ServiceContext> service_context,
                               std::unique_ptr<Request> request)
    : service_context_(service_context), request_(std::move(request)) {
  struct timezone tz;
  gettimeofday(&start_time_, &tz);
  operation_id_ = GenerateUUID();
  const std::string &method = request_->GetRequestHTTPMethod();
  const std::string &path = request_->GetRequestPath();
  std::string query_params = request_->GetQueryParameters();

  // In addition to matching the method, service_context_->GetMethodCallInfo()
  // will extract the variable bindings from the url. We need variable bindings
  // only when we need to do transcoding. If this turns out to be a performance
  // problem for non-transcoded calls, we have a couple of options:
  // 1) Do not extract variable bindings here, and do the method matching again
  //    with extracting variable bindings when transcoding is needed.
  // 2) Store all the pieces needed for extracting variable bindings (such as
  //    http template variables, url path parts) in MethodCallInfo and extract
  //    variables lazily when needed.
  method_call_ =
      service_context_->GetMethodCallInfo(method, path, query_params);

  if (method_call_.method_info) {
    ExtractApiKey();
  }
  request_->FindHeader("referer", &http_referer_);

  // Enable trace if tracing is not force disabled and the triggering header is
  // set.
  if (service_context_->cloud_trace_aggregator()) {
    std::string trace_context_header;
    request_->FindHeader(kCloudTraceContextHeader, &trace_context_header);

    std::string method_name = kUnrecognizedOperation;
    if (method_call_.method_info) {
      method_name = method_call_.method_info->selector();
    }
    // qualify with the service name
    method_name = service_context_->service_name() + "/" + method_name;
    cloud_trace_.reset(cloud_trace::CreateCloudTrace(
        trace_context_header, method_name,
        &service_context_->cloud_trace_aggregator()->sampler()));
  }
}

void RequestContext::ExtractApiKey() {
  bool api_key_defined = false;
  auto url_queries = method()->api_key_url_query_parameters();
  if (url_queries) {
    api_key_defined = true;
    for (const auto &url_query : *url_queries) {
      if (request_->FindQuery(url_query, &api_key_)) {
        return;
      }
    }
  }

  auto headers = method()->api_key_http_headers();
  if (headers) {
    api_key_defined = true;
    for (const auto &header : *headers) {
      if (request_->FindHeader(header, &api_key_)) {
        return;
      }
    }
  }

  if (!api_key_defined) {
    // If api_key is not specified for a method,
    // check "key" first, if not, check "api_key" in query parameter.
    if (!request_->FindQuery(kDefaultApiKeyQueryName1, &api_key_)) {
      if (!request_->FindQuery(kDefaultApiKeyQueryName2, &api_key_)) {
        request_->FindHeader(kDefaultApiKeyHeaderName, &api_key_);
      }
    }
  }
}

void RequestContext::CompleteCheck(Status status) {
  // Makes sure set_check_continuation() is called.
  // Only making sure CompleteCheck() is NOT called twice.
  GOOGLE_CHECK(check_continuation_);

  auto temp_continuation = check_continuation_;
  check_continuation_ = nullptr;

  temp_continuation(status);
}

void RequestContext::FillOperationInfo(service_control::OperationInfo *info) {
  if (method()) {
    info->operation_name = method()->selector();
  } else {
    info->operation_name = kUnrecognizedOperation;
  }
  info->operation_id = operation_id_;
  if (check_response_info_.is_api_key_valid) {
    info->api_key = api_key_;
  }
  info->producer_project_id = service_context()->project_id();
  info->referer = http_referer_;
  info->request_start_time = start_time_;
}

void RequestContext::FillLocation(service_control::ReportRequestInfo *info) {
  if (service_context()->gce_metadata()->has_valid_data() &&
      !service_context()->gce_metadata()->zone().empty()) {
    info->location = service_context()->gce_metadata()->zone();
  } else {
    info->location = kDefaultLocation;
  }
}

void RequestContext::FillComputePlatform(
    service_control::ReportRequestInfo *info) {
  compute_platform::ComputePlatform cp;

  GceMetadata *metadata = service_context()->gce_metadata();
  if (metadata == nullptr || !metadata->has_valid_data()) {
    cp = compute_platform::UNKNOWN;
  } else {
    if (!metadata->gae_server_software().empty()) {
      cp = compute_platform::GAE_FLEX;
    } else if (!metadata->kube_env().empty()) {
      cp = compute_platform::GKE;
    } else {
      cp = compute_platform::GCE;
    }
  }

  info->compute_platform = cp;
}

void RequestContext::FillLogMessage(service_control::ReportRequestInfo *info) {
  if (method()) {
    info->api_method = method()->selector();
    info->api_name = method()->api_name();
    info->api_version = method()->api_version();
    info->log_message = std::string(kMessage) + method()->selector();
  } else {
    std::string http_verb = info->method;
    if (http_verb.empty()) {
      http_verb = kUnknownHttpVerb;
    }
    info->log_message = std::string(kIgnoredMessage) + http_verb + " " +
                        request_->GetUnparsedRequestPath();
  }
}

void RequestContext::FillCheckRequestInfo(
    service_control::CheckRequestInfo *info) {
  FillOperationInfo(info);
  info->client_ip = request_->GetClientIP();
  info->allow_unregistered_calls = method()->allow_unregistered_calls();
}

void RequestContext::FillReportRequestInfo(
    Response *response, service_control::ReportRequestInfo *info) {
  FillOperationInfo(info);
  FillLocation(info);
  FillComputePlatform(info);

  info->url = request_->GetUnparsedRequestPath();
  info->method = request_->GetRequestHTTPMethod();

  info->request_size = response->GetRequestSize();
  info->response_size = response->GetResponseSize();
  info->status = response->GetResponseStatus();
  info->response_code = info->status.HttpCode();
  info->protocol = request_->GetRequestProtocol();
  info->check_response_info = check_response_info_;

  info->auth_issuer = auth_issuer_;
  info->auth_audience = auth_audience_;

  // Must be after response_code and method are assigned.
  FillLogMessage(info);

  response->GetLatencyInfo(&info->latency);
}

void RequestContext::StartBackendSpanAndSetTraceContext() {
  backend_span_.reset(CreateSpan(cloud_trace_.get(), "Backend"));

  // Set trace context header to backend. The span id in the header will
  // be the backend span's id.
  std::ostringstream trace_context_stream;
  trace_context_stream << cloud_trace()->trace()->trace_id() << "/"
                       << backend_span_->trace_span()->span_id() << ";"
                       << cloud_trace()->options();
  Status status = request()->AddHeaderToBackend(kCloudTraceContextHeader,
                                                trace_context_stream.str());
  if (!status.ok()) {
    service_context()->env()->LogError(
        "Failed to set trace context header to backend.");
  }
}

}  // namespace context
}  // namespace api_manager
}  // namespace google
