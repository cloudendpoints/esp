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
#include "src/api_manager/service_control/aggregated.h"

#include <sstream>
#include <typeinfo>
#include "src/api_manager/service_control/logs_metrics_loader.h"

using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::CheckResponse;
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api::servicecontrol::v1::ReportResponse;
using ::google::api_manager::proto::ServerConfig;
using ::google::api_manager::proto::ServiceControlClientConfig;
using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

using ::google::service_control_client::CheckAggregationOptions;
using ::google::service_control_client::ReportAggregationOptions;
using ::google::service_control_client::ServiceControlClient;
using ::google::service_control_client::ServiceControlClientOptions;
using ::google::service_control_client::TransportDoneFunc;

namespace google {
namespace api_manager {
namespace service_control {

namespace {

// Default config for check aggregator
const int kCheckAggregationEntries = 10000;
const int kCheckAggregationFlushIntervalMs = 1000;
const int kCheckAggregationExpirationMs = 60000;

// Default config for report aggregator
const int kReportAggregationEntries = 10000;
const int kReportAggregationFlushIntervalMs = 1000;

// The maximum protobuf pool size. All usages of pool alloc() and free() are
// within a function frame. If no con-current usage, pool size of 1 is enough.
// This number should correspond to maximum of concurrent calls.
const int kProtoPoolMaxSize = 100;

// Defines protobuf content type.
const char application_proto[] = "application/x-protobuf";

// The service_control service name. used for as audience to generate JWT token.
const char servicecontrol_service[] =
    "/google.api.servicecontrol.v1.ServiceController";

// Converts ::google::api_manager::Status to ::google::protobuf::util::Status
::google::protobuf::util::Status ConvertStatus(const Status& status) {
  return ::google::protobuf::util::Status(static_cast<Code>(status.code()),
                                          status.message());
}

// Converts ::google::protobuf::util::Status to utils::Status
Status ConvertStatus(const ::google::protobuf::util::Status& status) {
  return Status(status.error_code(), status.error_message());
}

// Generates CheckAggregationOptions.
CheckAggregationOptions GetCheckAggregationOptions(
    const ServerConfig* server_config) {
  if (server_config && server_config->has_service_control_client_config() &&
      server_config->service_control_client_config()
          .has_check_aggregator_config()) {
    const auto& check_config = server_config->service_control_client_config()
                                   .check_aggregator_config();
    return CheckAggregationOptions(check_config.cache_entries(),
                                   check_config.flush_interval_ms(),
                                   check_config.response_expiration_ms());
  }
  return CheckAggregationOptions(kCheckAggregationEntries,
                                 kCheckAggregationFlushIntervalMs,
                                 kCheckAggregationExpirationMs);
}

// Generates ReportAggregationOptions.
ReportAggregationOptions GetReportAggregationOptions(
    const ServerConfig* server_config) {
  if (server_config && server_config->has_service_control_client_config() &&
      server_config->service_control_client_config()
          .has_report_aggregator_config()) {
    const auto& report_config = server_config->service_control_client_config()
                                    .report_aggregator_config();
    return ReportAggregationOptions(report_config.cache_entries(),
                                    report_config.flush_interval_ms());
  }
  return ReportAggregationOptions(kReportAggregationEntries,
                                  kReportAggregationFlushIntervalMs);
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

Aggregated::Aggregated(const ::google::api::Service& service,
                       const ServerConfig* server_config,
                       ApiManagerEnvInterface* env,
                       auth::ServiceAccountToken* sa_token,
                       const std::set<std::string>& logs,
                       const std::set<std::string>& metrics,
                       const std::set<std::string>& labels)
    : service_(&service),
      server_config_(server_config),
      env_(env),
      sa_token_(sa_token),
      service_control_proto_(logs, metrics, labels),
      url_(service_, server_config) {
  if (sa_token_) {
    sa_token_->SetAudience(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_SERVICE_CONTROL,
        url_.service_control() + servicecontrol_service);
  }
}

Aggregated::Aggregated(const std::set<std::string>& logs,
                       ApiManagerEnvInterface* env,
                       std::unique_ptr<ServiceControlClient> client)
    : service_(nullptr),
      server_config_(nullptr),
      env_(env),
      sa_token_(nullptr),
      service_control_proto_(logs),
      url_(service_, server_config_),
      client_(std::move(client)) {}

Aggregated::~Aggregated() {}

Status Aggregated::Init() {
  // Init() can be called repeatedly.
  if (client_) {
    return Status::OK;
  }

  // It is too early to create client_ at constructor.
  // Client creation is calling env->StartPeriodicTimer.
  // env->StartPeriodicTimer doens't work at constructor.
  ServiceControlClientOptions options(
      GetCheckAggregationOptions(server_config_),
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

  options.check_transport = [this](
      const CheckRequest& request, CheckResponse* response,
      TransportDoneFunc on_done) { Call(request, response, on_done); };
  options.report_transport = [this](
      const ReportRequest& request, ReportResponse* response,
      TransportDoneFunc on_done) { Call(request, response, on_done); };

  options.periodic_timer = [this](int interval_ms,
                                  std::function<void()> callback)
      -> std::unique_ptr<::google::service_control_client::PeriodicTimer> {
        return std::unique_ptr<::google::service_control_client::PeriodicTimer>(
            new ApiManagerPeriodicTimer(env_->StartPeriodicTimer(
                std::chrono::milliseconds(interval_ms), callback)));
      };
  client_ = ::google::service_control_client::CreateServiceControlClient(
      service_->name(), options);
  return Status::OK;
}

Status Aggregated::Close() {
  // Just destroy the client to flush all its cache.
  client_.reset();
  return Status::OK;
}

Status Aggregated::Report(const ReportRequestInfo& info) {
  if (!client_) {
    return Status(Code::INVALID_ARGUMENT, "Client object is nullptr.");
  }
  auto request = report_pool_.Alloc();
  Status status = service_control_proto_.FillReportRequest(info, request.get());
  if (!status.ok()) {
    report_pool_.Free(std::move(request));
    return status;
  }
  ReportResponse* response = new ReportResponse;
  client_->Report(*request, response,
                  [response](const ::google::protobuf::util::Status& status) {
                    delete response;
                  });
  // There is no reference to request anymore at this point and it is safe to
  // free request now.
  report_pool_.Free(std::move(request));
  return Status::OK;
}

void Aggregated::Check(
    const CheckRequestInfo& info,
    std::function<void(Status, const CheckResponseInfo&)> on_done) {
  CheckResponseInfo dummy_response_info;
  if (!client_) {
    on_done(Status(Code::INVALID_ARGUMENT, "Client object is nullptr."),
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

  // Makes a copy of project_id since it needs to pass to lambda.
  std::string project_id = info.producer_project_id;
  CheckResponse* response = new CheckResponse;

  auto check_on_done = [response, project_id, on_done](
      const ::google::protobuf::util::Status& status) {
    CheckResponseInfo response_info;
    if (status.ok()) {
      Status status =
          Proto::ConvertCheckResponse(*response, project_id, &response_info);
      on_done(status, response_info);
    } else {
      on_done(ConvertStatus(status), response_info);
    }
    delete response;
  };

  client_->Check(*request, response, check_on_done);
  // There is no reference to request anymore at this point and it is safe to
  // free request now.
  check_pool_.Free(std::move(request));
}

Status Aggregated::GetStatistics(Statistics* esp_stat) const {
  if (!client_) {
    return Status(Code::INVALID_ARGUMENT, "Client object is nullptr.");
  }

  ::google::service_control_client::Statistics client_stat;
  ::google::protobuf::util::Status status =
      client_->GetStatistics(&client_stat);

  if (!status.ok()) {
    return ConvertStatus(status);
  }
  esp_stat->total_called_checks = client_stat.total_called_checks;
  esp_stat->send_checks_by_flush = client_stat.send_checks_by_flush;
  esp_stat->send_checks_in_flight = client_stat.send_checks_in_flight;
  esp_stat->total_called_reports = client_stat.total_called_reports;
  esp_stat->send_reports_by_flush = client_stat.send_reports_by_flush;
  esp_stat->send_reports_in_flight = client_stat.send_reports_in_flight;
  esp_stat->send_report_operations = client_stat.send_report_operations;

  return Status::OK;
}

template <class RequestType, class ResponseType>
void Aggregated::Call(const RequestType& request, ResponseType* response,
                      TransportDoneFunc on_done) {
  std::unique_ptr<HTTPRequest> http_request(new HTTPRequest(
      [response, on_done, this](Status status, std::string&& body) {
        if (status.ok()) {
          if (!response->ParseFromString(body)) {
            status = Status(Code::INVALID_ARGUMENT,
                            std::string("Invalid response: ") + body);
          }
        } else {
          const std::string& url = typeid(RequestType) == typeid(CheckRequest)
                                       ? url_.check_url()
                                       : url_.report_url();
          env_->LogError(std::string("Failed to call ") + url + ", Error: " +
                         status.ToString());
        }
        on_done(ConvertStatus(status));
      }));

  const std::string& url = typeid(RequestType) == typeid(CheckRequest)
                               ? url_.check_url()
                               : url_.report_url();

  std::string request_body;
  request.SerializeToString(&request_body);

  http_request->set_url(url)
      .set_method("POST")
      .set_auth_token(GetAuthToken())
      .set_header("Content-Type", application_proto)
      .set_body(request_body);

  // Set timeout on the request if it was so configured.
  if (server_config_ != nullptr &&
      server_config_->has_service_control_client_config()) {
    const auto& config = server_config_->service_control_client_config();
    if (config.timeout_ms() > 0) {
      http_request->set_timeout_ms(config.timeout_ms());
    }
  }

  Status status = env_->RunHTTPRequest(std::move(http_request));
  if (!status.ok()) {
    // Failed to send HTTPRequest.
    on_done(ConvertStatus(status));
  }
}

const std::string& Aggregated::GetAuthToken() {
  if (sa_token_) {
    return sa_token_->GetAuthToken(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_SERVICE_CONTROL);
  } else {
    static std::string empty;
    return empty;
  }
}

Interface* Aggregated::Create(const ::google::api::Service& service,
                              const ServerConfig* server_config,
                              ApiManagerEnvInterface* env,
                              auth::ServiceAccountToken* sa_token) {
  Url url(&service, server_config);
  if (url.service_control().empty()) {
    env->LogError(
        "Service control address is not specified. Disabling API management.");
    return nullptr;
  }
  std::set<std::string> logs, metrics, labels;
  Status s = LogsMetricsLoader::Load(service, &logs, &metrics, &labels);
  return new Aggregated(service, server_config, env, sa_token, logs, metrics,
                        labels);
}

}  // namespace service_control
}  // namespace api_manager
}  // namespace google
