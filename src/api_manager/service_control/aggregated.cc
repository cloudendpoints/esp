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
#include "src/api_manager/service_control/aggregated.h"

#include <sstream>
#include <typeinfo>
#include "src/api_manager/service_control/logs_metrics_loader.h"

using ::google::api::servicecontrol::v1::AllocateQuotaRequest;
using ::google::api::servicecontrol::v1::AllocateQuotaResponse;
using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::CheckResponse;
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api::servicecontrol::v1::ReportResponse;
using ::google::api_manager::proto::ServerConfig;
using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

using ::google::service_control_client::CheckAggregationOptions;
using ::google::service_control_client::QuotaAggregationOptions;
using ::google::service_control_client::ReportAggregationOptions;
using ::google::service_control_client::ServiceControlClient;
using ::google::service_control_client::ServiceControlClientOptions;
using ::google::service_control_client::TransportDoneFunc;

namespace google {
namespace api_manager {
namespace service_control {

namespace {

const int kQuotaAggregationEntries = 10000;
const int kQuotaAggregationRefreshMs = 1000;

// Default config for check aggregator
const int kCheckAggregationEntries = 10000;
// Check doesn't support quota yet. It is safe to increase
// the cache life of check results.
// Cache life is 5 minutes. It will be refreshed every minute.
const int kCheckAggregationFlushIntervalMs = 60000;
const int kCheckAggregationExpirationMs = 300000;

// Default config for report aggregator
const int kReportAggregationEntries = 10000;
const int kReportAggregationFlushIntervalMs = 1000;

// The default connection timeout for check requests.
// 1.5s Check timeout is based on SYN resend timeout is 1s.
const int kCheckDefaultTimeoutInMs = 1500;
// The default connection timeout for allocate quota requests.
// 1.5s Quota timeout is based on SYN resend timeout is 1s.
const int kAllocateQuotaDefaultTimeoutInMs = 1500;
// The default connection timeout for report requests.
// 3.5s Report timeout is based on SYN resend timeout is 1s.
// and Server processing Report is much slower.
const int kReportDefaultTimeoutInMs = 3500;

// The default number of retries for check calls.
const int kCheckDefaultNumberOfRetries = 3;
// The default number of retries for report calls.
const int kReportDefaultNumberOfRetries = 5;
// The default number of retries for allocate quota calls.
// Allocate quota has fail_open policy, retry once is enough.
const int kAllocateQuotaDefaultNumberOfRetries = 1;

// The maximum protobuf pool size. All usages of pool alloc() and free() are
// within a function frame. If no con-current usage, pool size of 1 is enough.
// This number should correspond to maximum of concurrent calls.
const int kProtoPoolMaxSize = 100;

// Defines protobuf content type.
const char application_proto[] = "application/x-protobuf";

// The service_control service name. used for as audience to generate JWT token.
const char servicecontrol_service[] =
    "/google.api.servicecontrol.v1.ServiceController";

// The quota_control service name. used for as audience to generate JWT token.
const char quotacontrol_service[] =
    "/google.api.servicecontrol.v1.QuotaController";

// Define network failure error codes:
// All 500 Http status codes are marked as network failure.
// Http status code is converted to Status::code as:
// https://github.com/cloudendpoints/esp/blob/master/src/api_manager/utils/status.cc#L364
// which is called by Status.ToProto() at Aggregated::Call() on_done function.
bool IsErrorCodeNetworkFailure(int code) {
  return code == Code::UNAVAILABLE || code == Code::INTERNAL ||
         code == Code::UNIMPLEMENTED || code == Code::DEADLINE_EXCEEDED;
}

// Generates CheckAggregationOptions.
CheckAggregationOptions GetCheckAggregationOptions(
    const ServerConfig* server_config) {
  if (server_config && server_config->has_service_control_config() &&
      server_config->service_control_config().has_check_aggregator_config()) {
    const auto& check_config =
        server_config->service_control_config().check_aggregator_config();
    return CheckAggregationOptions(check_config.cache_entries(),
                                   check_config.flush_interval_ms(),
                                   check_config.response_expiration_ms());
  }
  return CheckAggregationOptions(kCheckAggregationEntries,
                                 kCheckAggregationFlushIntervalMs,
                                 kCheckAggregationExpirationMs);
}

// Generate QuotaAggregationOptions
QuotaAggregationOptions GetQuotaAggregationOptions(
    const ServerConfig* server_config) {
  QuotaAggregationOptions option = QuotaAggregationOptions(
      kQuotaAggregationEntries, kQuotaAggregationRefreshMs);

  if (server_config && server_config->has_service_control_config() &&
      server_config->service_control_config().has_quota_aggregator_config()) {
    const auto& quota_config =
        server_config->service_control_config().quota_aggregator_config();

    option.num_entries = quota_config.cache_entries();
    option.refresh_interval_ms = quota_config.refresh_interval_ms();
  }

  return option;
}

// Generates ReportAggregationOptions.
ReportAggregationOptions GetReportAggregationOptions(
    const ServerConfig* server_config) {
  if (server_config && server_config->has_service_control_config() &&
      server_config->service_control_config().has_report_aggregator_config()) {
    const auto& report_config =
        server_config->service_control_config().report_aggregator_config();
    return ReportAggregationOptions(report_config.cache_entries(),
                                    report_config.flush_interval_ms());
  }
  return ReportAggregationOptions(kReportAggregationEntries,
                                  kReportAggregationFlushIntervalMs);
}

const std::string& GetEmptyString() {
  static const std::string* const kEmptyString = new std::string;
  return *kEmptyString;
}

}  // namespace

template <class Type>
std::unique_ptr<Type> Aggregated::ProtoPool<Type>::Alloc() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!pool_.empty()) {
    auto item = std::move(pool_.front());
    pool_.pop_front();
    item->Clear();
    return item;
  } else {
    return std::unique_ptr<Type>(new Type);
  }
}

template <class Type>
void Aggregated::ProtoPool<Type>::Free(std::unique_ptr<Type> item) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (pool_.size() < kProtoPoolMaxSize) {
    pool_.push_back(std::move(item));
  }
}

Aggregated::Aggregated(
    const ::google::api::Service& service, const ServerConfig* server_config,
    ApiManagerEnvInterface* env, auth::ServiceAccountToken* sa_token,
    const std::set<std::string>& logs, const std::set<std::string>& metrics,
    const std::set<std::string>& labels, SetRolloutIdFunc set_rollout_id_func)
    : service_(&service),
      server_config_(server_config),
      env_(env),
      sa_token_(sa_token),
      service_control_proto_(logs, metrics, labels, service.name(),
                             service.id()),
      url_(service_, server_config),
      mismatched_check_config_id_(service.id()),
      mismatched_report_config_id_(service.id()),
      max_report_size_(0),
      set_rollout_id_func_(set_rollout_id_func) {
  if (sa_token_) {
    sa_token_->SetAudience(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_SERVICE_CONTROL,
        url_.service_control() + servicecontrol_service);
    sa_token_->SetAudience(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_QUOTA_CONTROL,
        url_.service_control() + quotacontrol_service);
  }
}

Aggregated::Aggregated(const std::set<std::string>& logs,
                       ApiManagerEnvInterface* env,
                       std::unique_ptr<ServiceControlClient> client)
    : service_(nullptr),
      server_config_(nullptr),
      env_(env),
      sa_token_(nullptr),
      service_control_proto_(logs, "", ""),
      url_(service_, server_config_),
      client_(std::move(client)),
      max_report_size_(0) {}

Aggregated::~Aggregated() {}

void Aggregated::InitHttpRequestTimeoutRetries() {
  check_timeout_ms_ = kCheckDefaultTimeoutInMs;
  report_timeout_ms_ = kReportDefaultTimeoutInMs;
  quota_timeout_ms_ = kAllocateQuotaDefaultTimeoutInMs;
  check_retries_ = kCheckDefaultNumberOfRetries;
  report_retries_ = kReportDefaultNumberOfRetries;
  quota_retries_ = kAllocateQuotaDefaultNumberOfRetries;
  network_fail_open_ = false;

  if (server_config_ != nullptr &&
      server_config_->has_service_control_config()) {
    const auto& config = server_config_->service_control_config();
    if (config.check_timeout_ms() > 0) {
      check_timeout_ms_ = config.check_timeout_ms();
    }
    if (config.report_timeout_ms() > 0) {
      report_timeout_ms_ = config.report_timeout_ms();
    }
    if (config.quota_timeout_ms() > 0) {
      quota_timeout_ms_ = config.quota_timeout_ms();
    }
    if (config.check_retries() > 0) {
      check_retries_ = config.check_retries();
    }
    if (config.report_retries() > 0) {
      report_retries_ = config.report_retries();
    }
    if (config.quota_retries() > 0) {
      quota_retries_ = config.quota_retries();
    }
    network_fail_open_ = config.network_fail_open();
  }
}

Status Aggregated::Init() {
  // Init() can be called repeatedly.
  if (client_) {
    return Status::OK;
  }

  InitHttpRequestTimeoutRetries();

  // It is too early to create client_ at constructor.
  // Client creation is calling env->StartPeriodicTimer.
  // env->StartPeriodicTimer doens't work at constructor.
  ServiceControlClientOptions options(
      GetCheckAggregationOptions(server_config_),
      GetQuotaAggregationOptions(server_config_),
      GetReportAggregationOptions(server_config_));

  std::stringstream ss;
  ss << "Check_aggregation_options: "
     << "num_entries: " << options.check_options.num_entries
     << ", flush_interval_ms: " << options.check_options.flush_interval_ms
     << ", expiration_ms: " << options.check_options.expiration_ms
     << ", Report_aggregation_options: "
     << "num_entries: " << options.report_options.num_entries
     << ", flush_interval_ms: " << options.report_options.flush_interval_ms;
  env_->LogInfo(ss.str().c_str());

  options.check_transport = [this](const CheckRequest& request,
                                   CheckResponse* response,
                                   TransportDoneFunc on_done) {
    Call(request, response, on_done, nullptr);
  };

  options.quota_transport = [this](const AllocateQuotaRequest& request,
                                   AllocateQuotaResponse* response,
                                   TransportDoneFunc on_done) {
    Call(request, response, on_done, nullptr);
  };

  options.report_transport = [this](const ReportRequest& request,
                                    ReportResponse* response,
                                    TransportDoneFunc on_done) {
    Call(request, response, on_done, nullptr);
  };

  options.periodic_timer = [this](int interval_ms,
                                  std::function<void()> callback)
      -> std::unique_ptr<::google::service_control_client::PeriodicTimer> {
    return std::unique_ptr<::google::service_control_client::PeriodicTimer>(
        new ApiManagerPeriodicTimer(env_->StartPeriodicTimer(
            std::chrono::milliseconds(interval_ms), callback)));
  };
  client_ = ::google::service_control_client::CreateServiceControlClient(
      service_->name(), service_->id(), options);
  return Status::OK;
}

Status Aggregated::Close() {
  // Just destroy the client to flush all its cache.
  client_.reset();
  return Status::OK;
}

void Aggregated::SendEmptyReport() {
  ReportRequest request;
  ReportResponse* response = new ReportResponse;
  Call(request, response,
       [this, response](const ::google::protobuf::util::Status&) {
         delete response;
       },
       nullptr);
}

Status Aggregated::Report(const ReportRequestInfo& info) {
  if (!client_) {
    return Status(Code::INTERNAL, "Missing service control client");
  }
  auto request = report_pool_.Alloc();
  Status status = service_control_proto_.FillReportRequest(info, request.get());
  if (!status.ok()) {
    report_pool_.Free(std::move(request));
    return status;
  }
  ReportResponse* response = new ReportResponse;
  client_->Report(
      *request, response,
      [this, response](const ::google::protobuf::util::Status& status) {
        if (!status.ok() && env_) {
          env_->LogError(std::string("Service control report failed. " +
                                     status.ToString()));
        }
        delete response;
      });
  // There is no reference to request anymore at this point and it is safe to
  // free request now.
  report_pool_.Free(std::move(request));
  return Status::OK;
}

void Aggregated::Check(
    const CheckRequestInfo& info, cloud_trace::CloudTraceSpan* parent_span,
    std::function<void(Status, const CheckResponseInfo&)> on_done) {
  std::shared_ptr<cloud_trace::CloudTraceSpan> trace_span(
      CreateChildSpan(parent_span, "CheckServiceControlCache"));
  CheckResponseInfo dummy_response_info;
  if (!client_) {
    on_done(Status(Code::INTERNAL, "Missing service control client"),
            dummy_response_info);
    return;
  }
  auto request = check_pool_.Alloc();
  Status status = service_control_proto_.FillCheckRequest(info, request.get());
  if (!status.ok()) {
    on_done(status, dummy_response_info);
    check_pool_.Free(std::move(request));
    return;
  }

  CheckResponse* response = new CheckResponse;

  auto check_on_done = [this, response, on_done, trace_span](
                           const ::google::protobuf::util::Status& status) {
    TRACE(trace_span) << "Check returned with status: " << status.ToString();
    CheckResponseInfo response_info;
    if (status.ok()) {
      Status status = Proto::ConvertCheckResponse(
          *response, service_control_proto_.service_name(), &response_info);
      on_done(status, response_info);
    } else {
      // If network_fail_open is true, it is OK to proceed
      if (network_fail_open_ &&
          IsErrorCodeNetworkFailure(status.error_code())) {
        env_->LogError(
            std::string("With network fail open policy, the request is allowed "
                        "even the service control check failed with: " +
                        status.ToString()));
        on_done(Status::OK, response_info);
      } else {
        on_done(Status(status.error_code(), status.error_message(),
                       Status::SERVICE_CONTROL),
                response_info);
      }
    }
    delete response;
  };

  client_->Check(
      *request, response, check_on_done,
      [trace_span, this](const CheckRequest& request, CheckResponse* response,
                         TransportDoneFunc on_done) {
        Call(request, response, on_done, trace_span.get());
      });
  // There is no reference to request anymore at this point and it is safe to
  // free request now.
  check_pool_.Free(std::move(request));
}

void Aggregated::Quota(const QuotaRequestInfo& info,
                       cloud_trace::CloudTraceSpan* parent_span,
                       std::function<void(utils::Status)> on_done) {
  std::shared_ptr<cloud_trace::CloudTraceSpan> trace_span(
      CreateChildSpan(parent_span, "QuotaServiceControlCache"));

  if (!client_) {
    on_done(Status(Code::INTERNAL, "Missing service control client"));
    return;
  }

  auto request = quota_pool_.Alloc();

  Status status =
      service_control_proto_.FillAllocateQuotaRequest(info, request.get());
  if (!status.ok()) {
    on_done(status);
    quota_pool_.Free(std::move(request));
    return;
  }

  AllocateQuotaResponse* response = new AllocateQuotaResponse();

  auto quota_on_done = [this, response, on_done, trace_span](
                           const ::google::protobuf::util::Status& status) {
    TRACE(trace_span) << "AllocateQuotaRequst returned with status: "
                      << status.ToString();

    if (status.ok()) {
      on_done(Proto::ConvertAllocateQuotaResponse(
          *response, service_control_proto_.service_name()));
    } else {
      on_done(Status(status.error_code(), status.error_message(),
                     Status::SERVICE_CONTROL));
    }

    delete response;
  };

  client_->Quota(*request, response, quota_on_done,
                 [trace_span, this](const AllocateQuotaRequest& request,
                                    AllocateQuotaResponse* response,
                                    TransportDoneFunc on_done) {
                   Call(request, response, on_done, trace_span.get());
                 });

  // There is no reference to request anymore at this point and it is safe to
  // free request now.
  quota_pool_.Free(std::move(request));
}

Status Aggregated::GetStatistics(Statistics* esp_stat) const {
  if (!client_) {
    return Status(Code::INTERNAL, "Missing service control client");
  }

  ::google::service_control_client::Statistics client_stat;
  ::google::protobuf::util::Status status =
      client_->GetStatistics(&client_stat);

  if (!status.ok()) {
    return Status::FromProto(status);
  }
  esp_stat->total_called_checks = client_stat.total_called_checks;
  esp_stat->send_checks_by_flush = client_stat.send_checks_by_flush;
  esp_stat->send_checks_in_flight = client_stat.send_checks_in_flight;
  esp_stat->total_called_reports = client_stat.total_called_reports;
  esp_stat->send_reports_by_flush = client_stat.send_reports_by_flush;
  esp_stat->send_reports_in_flight = client_stat.send_reports_in_flight;
  esp_stat->send_report_operations = client_stat.send_report_operations;
  esp_stat->max_report_size = max_report_size_;

  return Status::OK;
}

template <>
const std::string& Aggregated::GetApiRequestUrl<CheckRequest>() {
  return url_.check_url();
}
template <>
const std::string& Aggregated::GetApiRequestUrl<ReportRequest>() {
  return url_.report_url();
}
template <>
const std::string& Aggregated::GetApiRequestUrl<AllocateQuotaRequest>() {
  return url_.quota_url();
}

template <>
int Aggregated::GetHttpRequestTimeout<CheckRequest>() {
  return check_timeout_ms_;
}
template <>
int Aggregated::GetHttpRequestTimeout<ReportRequest>() {
  return report_timeout_ms_;
}
template <>
int Aggregated::GetHttpRequestTimeout<AllocateQuotaRequest>() {
  return quota_timeout_ms_;
}

template <>
int Aggregated::GetHttpRequestRetries<CheckRequest>() {
  return check_retries_;
}
template <>
int Aggregated::GetHttpRequestRetries<ReportRequest>() {
  return report_retries_;
}
template <>
int Aggregated::GetHttpRequestRetries<AllocateQuotaRequest>() {
  return quota_retries_;
}

template <>
const std::string& Aggregated::GetAuthToken<CheckRequest>() {
  if (sa_token_) {
    return sa_token_->GetAuthToken(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_SERVICE_CONTROL);
  } else {
    return GetEmptyString();
  }
}
template <>
const std::string& Aggregated::GetAuthToken<ReportRequest>() {
  if (sa_token_) {
    return sa_token_->GetAuthToken(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_SERVICE_CONTROL);
  } else {
    return GetEmptyString();
  }
}
template <>
const std::string& Aggregated::GetAuthToken<AllocateQuotaRequest>() {
  if (sa_token_) {
    return sa_token_->GetAuthToken(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_QUOTA_CONTROL);
  } else {
    return GetEmptyString();
  }
}

template <>
void Aggregated::HandleResponse(const CheckResponse& response) {
  if (set_rollout_id_func_ && !response.service_rollout_id().empty()) {
    set_rollout_id_func_(response.service_rollout_id());
  }

  if (!response.service_config_id().empty() &&
      service_control_proto_.service_config_id() !=
          response.service_config_id()) {
    if (mismatched_check_config_id_ != response.service_config_id()) {
      env_->LogDebug(
          "Received non-matching check response service config ID: '" +
          response.service_config_id() + "', requested: '" +
          service_control_proto_.service_config_id() + "'");
      mismatched_check_config_id_ = response.service_config_id();
    }
  }
}

template <>
void Aggregated::HandleResponse(const ReportResponse& response) {
  if (set_rollout_id_func_ && !response.service_rollout_id().empty()) {
    set_rollout_id_func_(response.service_rollout_id());
  }

  if (!response.service_config_id().empty() &&
      service_control_proto_.service_config_id() !=
          response.service_config_id()) {
    if (mismatched_report_config_id_ != response.service_config_id()) {
      env_->LogDebug(
          "Received non-matching report response service config ID: '" +
          response.service_config_id() + "', requested: '" +
          service_control_proto_.service_config_id() + "'");
      mismatched_report_config_id_ = response.service_config_id();
    }
  }
}

template <>
void Aggregated::HandleResponse(const AllocateQuotaResponse& response) {}

template <class RequestType, class ResponseType>
void Aggregated::Call(const RequestType& request, ResponseType* response,
                      TransportDoneFunc on_done,
                      cloud_trace::CloudTraceSpan* parent_span) {
  std::shared_ptr<cloud_trace::CloudTraceSpan> trace_span(
      CreateChildSpan(parent_span, "Call ServiceControl server"));

  const std::string& url = GetApiRequestUrl<RequestType>();
  TRACE(trace_span) << "Http request URL: " << url;

  std::unique_ptr<HTTPRequest> http_request(
      new HTTPRequest([url, response, on_done, trace_span, this](
                          Status status, std::map<std::string, std::string>&&,
                          std::string&& body) {
        TRACE(trace_span) << "HTTP response status: " << status.ToString();
        if (status.ok()) {
          // Handle 200 response
          if (!response->ParseFromString(body)) {
            status =
                Status(Code::INVALID_ARGUMENT, std::string("Invalid response"));
          }
          HandleResponse(*response);
        } else {
          env_->LogError(std::string("Failed to call ") + url + ", Error: " +
                         status.ToString() + ", Response body: " + body);

          // Handle NGX error as opposed to pass-through error code
          if (status.code() < 0) {
            status = Status(Code::UNAVAILABLE,
                            "Failed to connect to service control");
          } else {
            std::string error_msg;
            if (body.empty()) {
              error_msg =
                  "Service control request failed with HTTP response code " +
                  std::to_string(status.code());
            } else {
              // Pass the body as error message to client.
              error_msg = body;
            }
            status = Status(status.code(), error_msg);
          }
        }
        on_done(status.ToProto());
      }));

  std::string request_body;
  request.SerializeToString(&request_body);

  // Collect statistics on the maximum report body size.
  if ((typeid(RequestType) == typeid(ReportRequest)) &&
      (request_body.size() > max_report_size_)) {
    max_report_size_ = request_body.size();
  }

  http_request->set_url(url)
      .set_method("POST")
      .set_auth_token(GetAuthToken<RequestType>())
      .set_header("Content-Type", application_proto)
      .set_body(request_body);

  http_request->set_timeout_ms(GetHttpRequestTimeout<RequestType>());
  http_request->set_max_retries(GetHttpRequestRetries<RequestType>());

  env_->RunHTTPRequest(std::move(http_request));
}

Interface* Aggregated::Create(const ::google::api::Service& service,
                              const ServerConfig* server_config,
                              ApiManagerEnvInterface* env,
                              auth::ServiceAccountToken* sa_token,
                              SetRolloutIdFunc set_rollout_id_func) {
  if (server_config &&
      server_config->service_control_config().force_disable()) {
    env->LogError("Service control is disabled.");
    return nullptr;
  }
  Url url(&service, server_config);
  if (url.service_control().empty()) {
    env->LogError(
        "Service control address is not specified. Disabling API management.");
    return nullptr;
  }
  std::set<std::string> logs, metrics, labels;
  Status s = LogsMetricsLoader::Load(service, &logs, &metrics, &labels);
  return new Aggregated(service, server_config, env, sa_token, logs, metrics,
                        labels, set_rollout_id_func);
}

}  // namespace service_control
}  // namespace api_manager
}  // namespace google
