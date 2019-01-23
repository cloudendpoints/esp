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

#include "src/api_manager/context/request_context.h"
#include "google/api/backend.pb.h"
#include "google/protobuf/stubs/strutil.h"

#include <uuid/uuid.h>
#include <numeric>
#include <sstream>
#include <vector>

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {
namespace context {

namespace {

// Cloud Trace Context Header
const char kCloudTraceContextHeader[] = "X-Cloud-Trace-Context";

// Authorization Header
const char kAuthorizationHeader[] = "Authorization";

const char kBearerPrefix[] = "Bearer ";

// HTTP Method Override Header
const char kHttpMethodOverrideHeader[] = "X-HTTP-Method-Override";

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

// Delimiter of the IP addresses in the XFF header
const char kClientIPHeaderDelimeter = ',';

// Header for android package name, used for api key restriction check.
const char kXAndroidPackage[] = "x-android-package";

// Header for android certificate fingerprint, used for api key restriction
// check.
const char kXAndroidCert[] = "x-android-cert";

// Header for IOS bundle identifier, used for api key restriction check.
const char kXIosBundleId[] = "x-ios-bundle-identifier";

// Genereates a UUID string
std::string GenerateUUID() {
  char uuid_buf[kMaxUUIDBufSize];
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse(uuid, uuid_buf);
  return uuid_buf;
}

inline void split(const std::string &s, char delim,
                  std::vector<std::string> *elems) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    elems->push_back(item);
  }
}

inline const std::string trim(std::string &str) {
  str.erase(0, str.find_first_not_of(' '));  // heading spaces
  str.erase(str.find_last_not_of(' ') + 1);  // tailing spaces
  return str;
}

}  // namespace

using context::ServiceContext;

RequestContext::RequestContext(std::shared_ptr<ServiceContext> service_context,
                               std::unique_ptr<Request> request)
    : service_context_(service_context),
      request_(std::move(request)),
      is_first_report_(true),
      last_request_bytes_(0),
      last_response_bytes_(0) {
  start_time_ = std::chrono::system_clock::now();
  last_report_time_ = std::chrono::steady_clock::now();
  operation_id_ = GenerateUUID();
  const std::string &method = GetRequestHTTPMethodWithOverride();
  const std::string &path = request_->GetUnparsedRequestPath();
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

  if (method_call_.method_info &&
      !method_call_.method_info->allow_unregistered_calls()) {
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

std::string RequestContext::GetRequestHTTPMethodWithOverride() {
  std::string method;

  if (!request_->FindHeader(kHttpMethodOverrideHeader, &method)) {
    method = request()->GetRequestHTTPMethod();
  }

  service_context()->env()->LogDebug(std::string("Request method SET TO: ") +
                                     method);

  return method;
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
  if (check_response_info_.is_api_key_valid &&
      check_response_info_.service_is_activated) {
    info->api_key = api_key_;
  }
  info->producer_project_id = service_context()->project_id();
  info->referer = http_referer_;
  info->request_start_time = start_time_;
  info->client_ip = FindClientIPAddress();
  info->client_host = request_->GetClientHost();
}

void RequestContext::FillLocation(service_control::ReportRequestInfo *info) {
  info->location = service_context()->global_context()->location();
}

void RequestContext::FillComputePlatform(
    service_control::ReportRequestInfo *info) {
  info->compute_platform = service_context()->global_context()->platform();
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
  info->allow_unregistered_calls = method()->allow_unregistered_calls();

  request_->FindHeader(kXAndroidPackage, &info->android_package_name);
  request_->FindHeader(kXAndroidCert, &info->android_cert_fingerprint);
  request_->FindHeader(kXIosBundleId, &info->ios_bundle_id);
}

void RequestContext::FillAllocateQuotaRequestInfo(
    service_control::QuotaRequestInfo *info) {
  FillOperationInfo(info);

  info->method_name = this->method_call_.method_info->name();
  info->metric_cost_vector =
      &this->method_call_.method_info->metric_cost_vector();
}

void RequestContext::FillReportRequestInfo(
    Response *response, service_control::ReportRequestInfo *info) {
  FillOperationInfo(info);
  FillLocation(info);
  FillComputePlatform(info);

  info->url = request_->GetUnparsedRequestPath();
  info->method = GetRequestHTTPMethodWithOverride();

  info->frontend_protocol = request_->GetFrontendProtocol();
  info->backend_protocol = request_->GetBackendProtocol();
  info->check_response_info = check_response_info_;

  info->auth_issuer = auth_issuer_;
  info->auth_audience = auth_audience_;

  if (!info->is_final_report) {
    // Make sure we send delta metrics for intermediate reports.
    info->request_bytes = request_->GetGrpcRequestBytes() - last_request_bytes_;
    info->response_bytes =
        request_->GetGrpcResponseBytes() - last_response_bytes_;
    last_request_bytes_ += info->request_bytes;
    last_response_bytes_ += info->response_bytes;
  } else {
    info->request_size = response->GetRequestSize();
    info->response_size = response->GetResponseSize();
    info->request_bytes = info->request_size - last_request_bytes_;
    if (info->request_bytes < 0) {
      info->request_bytes = 0;
    }
    info->response_bytes = info->response_size - last_response_bytes_;
    if (info->response_bytes < 0) {
      info->response_bytes = 0;
    }

    info->streaming_request_message_counts =
        request_->GetGrpcRequestMessageCounts();
    info->streaming_response_message_counts =
        request_->GetGrpcResponseMessageCounts();

    info->streaming_durations =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now() - start_time_)
            .count();

    info->status = response->GetResponseStatus();
    info->response_code = info->status.HttpCode();

    // Must be after response_code and method are assigned.
    FillLogMessage(info);
    bool is_streaming = false;
    if (method() &&
        (method()->request_streaming() || method()->response_streaming())) {
      is_streaming = true;
    }
    if (!is_streaming) {
      response->GetLatencyInfo(&info->latency);
    }
  }
}

const std::string RequestContext::FindClientIPAddress() {
  auto serverConfig = service_context_->config()->server_config();
  std::string client_ip_header;

  if (serverConfig->has_client_ip_extraction_config() &&
      serverConfig->client_ip_extraction_config().client_ip_header().length() >
          0 &&
      request_->FindHeader(
          serverConfig->client_ip_extraction_config().client_ip_header(),
          &client_ip_header)) {
    // split headers
    std::vector<std::string> secments;
    split(client_ip_header, kClientIPHeaderDelimeter, &secments);
    int client_ip_header_position =
        serverConfig->client_ip_extraction_config().client_ip_position();

    if (client_ip_header_position < 0) {
      client_ip_header_position = secments.size() + client_ip_header_position;
    }

    if (client_ip_header_position >= 0 &&
        client_ip_header_position < (int)secments.size()) {
      return trim(secments[client_ip_header_position]);
    }
  }

  return request_->GetClientIP();
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

std::string RequestContext::GetAuthorizationUrl() const {
  if (method_call_.method_info == nullptr) {
    return "";
  }
  if (auth_issuer_.empty()) {
    return method_call_.method_info->first_authorization_url();
  } else {
    return method_call_.method_info->authorization_url_by_issuer(auth_issuer_);
  }
}

std::string RequestContext::GetBackendPath() const {
  if (method_call_.method_info == nullptr) {
    return "";
  }

  if (method_call_.method_info->backend_path_translation() ==
      ::google::api::BackendRule_PathTranslation_APPEND_PATH_TO_ADDRESS) {
    return "";
  } else if (method_call_.method_info->backend_path_translation() ==
             ::google::api::BackendRule_PathTranslation_CONSTANT_ADDRESS) {
    std::string parameters;
    for (std::size_t i = 0; i != method_call_.variable_bindings.size(); i++) {
      auto &variable_binding = method_call_.variable_bindings[i];
      if (variable_binding.field_path.size() == 1) {
        parameters += variable_binding.field_path[0];
      } else if (variable_binding.field_path.size() > 1) {
        parameters = std::accumulate(variable_binding.field_path.begin(),
                                     variable_binding.field_path.end(),
                                     std::string("."));
      }
      parameters.append("=");
      parameters.append(variable_binding.value);
      if (i != method_call_.variable_bindings.size() - 1) {
        parameters.append("&");
      }
    }

    if (parameters != "") {
      return method_call_.method_info->backend_path() + "?" + parameters;
    }
    return method_call_.method_info->backend_path();
  } else {
    return "";
  }
}

bool RequestContext::ShouldOverrideBackend() const {
  if (method_call_.method_info == nullptr) {
    return false;
  }

  if (method_call_.method_info->backend_path_translation() ==
      ::google::api::BackendRule_PathTranslation_PATH_TRANSLATION_UNSPECIFIED) {
    return false;
  }
  return true;
}

void RequestContext::AddInstanceIdentityToken() {
  if (!method()) {
    return;
  }

  const auto &audience = method()->backend_jwt_audience();
  if (!audience.empty()) {
    auto &token = service_context()
                      ->global_context()
                      ->GetInstanceIdentityToken(audience)
                      ->GetAuthToken();
    if (!token.empty()) {
      Status status = request()->AddHeaderToBackend(kAuthorizationHeader,
                                                    kBearerPrefix + token);
      if (!status.ok()) {
        service_context()->env()->LogError(
            "Failed to set authorization header to backend.");
      }
    }
  }
}

}  // namespace context
}  // namespace api_manager
}  // namespace google
