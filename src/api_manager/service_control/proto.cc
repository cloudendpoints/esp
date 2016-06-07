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
#include "src/api_manager/service_control/proto.h"

#include <functional>

#include <sys/time.h>
#include <time.h>

#include "google/api/metric.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "include/api_manager/service_control.h"
#include "src/api_manager/auth/lib/auth_token.h"
#include "src/api_manager/auth/lib/base64.h"
#include "third_party/service-control-client-cxx/utils/distribution_helper.h"

using ::google::api::servicecontrol::v1::CheckError;
using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::CheckResponse;
using ::google::api::servicecontrol::v1::Distribution;
using ::google::api::servicecontrol::v1::LogEntry;
using ::google::api::servicecontrol::v1::MetricValue;
using ::google::api::servicecontrol::v1::MetricValueSet;
using ::google::api::servicecontrol::v1::Operation;
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api_manager::utils::Status;
using ::google::protobuf::Map;
using ::google::protobuf::StringPiece;
using ::google::protobuf::Timestamp;
using ::google::protobuf::util::error::Code;
using ::google::service_control_client::DistributionHelper;

namespace google {
namespace api_manager {
namespace service_control {

struct SupportedMetric {
  const char* name;
  ::google::api::MetricDescriptor_MetricKind metric_kind;
  ::google::api::MetricDescriptor_ValueType value_type;
  Status (*set)(const SupportedMetric& m, const ReportRequestInfo& info,
                Operation* operation);
};

struct SupportedLabel {
  const char* name;
  ::google::api::LabelDescriptor_ValueType value_type;

  enum Kind { USER = 0, SYSTEM = 1 };
  Kind kind;

  Status (*set)(const SupportedLabel& l, const ReportRequestInfo& info,
                Map<std::string, std::string>* labels);
};

namespace {

// Metric Helpers

MetricValue* AddMetricValue(const char* metric_name, Operation* operation) {
  MetricValueSet* metric_value_set = operation->add_metric_value_sets();
  metric_value_set->set_metric_name(metric_name);
  return metric_value_set->add_metric_values();
}

void AddInt64Metric(const char* metric_name, int64_t value,
                    Operation* operation) {
  MetricValue* metric_value = AddMetricValue(metric_name, operation);
  metric_value->set_int64_value(value);
}

// The parameters to initialize DistributionHelper
struct DistributionHelperOptions {
  int buckets;
  double growth;
  double scale;
};

const DistributionHelperOptions time_distribution = {8, 10.0, 1e-6};
const DistributionHelperOptions size_distribution = {8, 10.0, 1};
const double kMsToSecs = 1e-3;

Status AddDistributionMetric(const DistributionHelperOptions& options,
                             const char* metric_name, double value,
                             Operation* operation) {
  MetricValue* metric_value = AddMetricValue(metric_name, operation);
  Distribution distribution;
  ::google::protobuf::util::Status proto_status =
      DistributionHelper::InitExponential(options.buckets, options.growth,
                                          options.scale, &distribution);
  if (!proto_status.ok()) return Status::FromProto(proto_status);
  proto_status = DistributionHelper::AddSample(value, &distribution);
  if (!proto_status.ok()) return Status::FromProto(proto_status);
  *metric_value->mutable_distribution_value() = distribution;
  return Status::OK;
}

// Metrics supported by ESP.

Status set_int64_metric_to_constant_1(const SupportedMetric& m,
                                      const ReportRequestInfo& info,
                                      Operation* operation) {
  AddInt64Metric(m.name, 1l, operation);
  return Status::OK;
}

Status set_int64_metric_to_constant_1_if_http_error(
    const SupportedMetric& m, const ReportRequestInfo& info,
    Operation* operation) {
  // Use status code >= 400 to determine request failed.
  if (info.response_code >= 400) {
    AddInt64Metric(m.name, 1l, operation);
  }
  return Status::OK;
}

Status set_distribution_metric_to_request_size(const SupportedMetric& m,
                                               const ReportRequestInfo& info,
                                               Operation* operation) {
  if (info.request_size >= 0) {
    return AddDistributionMetric(size_distribution, m.name, info.request_size,
                                 operation);
  }
  return Status::OK;
}

Status set_distribution_metric_to_response_size(const SupportedMetric& m,
                                                const ReportRequestInfo& info,
                                                Operation* operation) {
  if (info.response_size >= 0) {
    return AddDistributionMetric(size_distribution, m.name, info.response_size,
                                 operation);
  }
  return Status::OK;
}

// TODO: Consider refactoring following 3 functions to avoid duplicate code
Status set_distribution_metric_to_request_time(const SupportedMetric& m,
                                               const ReportRequestInfo& info,
                                               Operation* operation) {
  if (info.latency.request_time_ms >= 0) {
    double request_time_secs = info.latency.request_time_ms * kMsToSecs;
    return AddDistributionMetric(time_distribution, m.name, request_time_secs,
                                 operation);
  }
  return Status::OK;
}

Status set_distribution_metric_to_backend_time(const SupportedMetric& m,
                                               const ReportRequestInfo& info,
                                               Operation* operation) {
  if (info.latency.backend_time_ms >= 0) {
    double backend_time_secs = info.latency.backend_time_ms * kMsToSecs;
    return AddDistributionMetric(time_distribution, m.name, backend_time_secs,
                                 operation);
  }
  return Status::OK;
}

Status set_distribution_metric_to_overhead_time(const SupportedMetric& m,
                                                const ReportRequestInfo& info,
                                                Operation* operation) {
  if (info.latency.overhead_time_ms >= 0) {
    double overhead_time_secs = info.latency.overhead_time_ms * kMsToSecs;
    return AddDistributionMetric(time_distribution, m.name, overhead_time_secs,
                                 operation);
  }
  return Status::OK;
}

// Currently unsupported metrics:
//
//  "serviceruntime.googleapis.com/api/producer/by_consumer/quota_used_count"
//
const SupportedMetric supported_metrics[] = {
    {
        "serviceruntime.googleapis.com/api/consumer/request_count",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_INT64,
        set_int64_metric_to_constant_1,
    },
    {
        "serviceruntime.googleapis.com/api/producer/request_count",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_INT64,
        set_int64_metric_to_constant_1,
    },
    {
        "serviceruntime.googleapis.com/api/producer/by_consumer/request_count",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_INT64,
        set_int64_metric_to_constant_1,
    },
    {
        "serviceruntime.googleapis.com/api/consumer/request_sizes",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_request_size,
    },
    {
        "serviceruntime.googleapis.com/api/producer/request_sizes",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_request_size,
    },
    {
        "serviceruntime.googleapis.com/api/producer/by_consumer/request_sizes",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_request_size,
    },
    {
        "serviceruntime.googleapis.com/api/consumer/response_sizes",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_response_size,
    },
    {
        "serviceruntime.googleapis.com/api/producer/response_sizes",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_response_size,
    },
    {
        "serviceruntime.googleapis.com/api/producer/by_consumer/response_sizes",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_response_size,
    },
    {
        "serviceruntime.googleapis.com/api/consumer/error_count",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_INT64,
        set_int64_metric_to_constant_1_if_http_error,
    },
    {
        "serviceruntime.googleapis.com/api/producer/error_count",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_INT64,
        set_int64_metric_to_constant_1_if_http_error,
    },
    {
        "serviceruntime.googleapis.com/api/producer/by_consumer/error_count",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_INT64,
        set_int64_metric_to_constant_1_if_http_error,
    },
    {
        "serviceruntime.googleapis.com/api/consumer/total_latencies",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_request_time,
    },
    {
        "serviceruntime.googleapis.com/api/producer/total_latencies",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_request_time,
    },
    {
        "serviceruntime.googleapis.com/api/producer/by_consumer/"
        "total_latencies",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_request_time,
    },
    {
        "serviceruntime.googleapis.com/api/consumer/backend_latencies",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_backend_time,
    },
    {
        "serviceruntime.googleapis.com/api/producer/backend_latencies",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_backend_time,
    },
    {
        "serviceruntime.googleapis.com/api/producer/by_consumer/"
        "backend_latencies",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_backend_time,
    },
    {
        "serviceruntime.googleapis.com/api/consumer/request_overhead_latencies",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_overhead_time,
    },
    {
        "serviceruntime.googleapis.com/api/producer/request_overhead_latencies",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_overhead_time,
    },
    {
        "serviceruntime.googleapis.com/api/producer/by_consumer/"
        "request_overhead_latencies",
        ::google::api::MetricDescriptor_MetricKind_DELTA,
        ::google::api::MetricDescriptor_ValueType_DISTRIBUTION,
        set_distribution_metric_to_overhead_time,
    },
};
const int supported_metrics_count =
    sizeof(supported_metrics) / sizeof(supported_metrics[0]);

const char kServiceControlCallerIp[] =
    "servicecontrol.googleapis.com/caller_ip";
const char kServiceControlReferer[] = "servicecontrol.googleapis.com/referer";
const char kServiceControlServiceAgent[] =
    "servicecontrol.googleapis.com/service_agent";
const char kServiceControlUserAgent[] =
    "servicecontrol.googleapis.com/user_agent";
const char kServiceControlPlatform[] = "servicecontrol.googleapis.com/platform";

// User agent label value
// The value for kUserAgent should be configured at service control server.
// Now it is configured as "ESP".
const char kUserAgent[] = "ESP";

// Service agent label value
const char kServiceAgent[] = "ESP";

// /credential_id
Status set_credential_id(const SupportedLabel& l, const ReportRequestInfo& info,
                         Map<std::string, std::string>* labels) {
  // The rule to set /credential_id is:
  // 1) If api_key is available, set it as apiKey:API-KEY
  // 2) If auth issuer and audience both are available, set it as:
  //    jwtAuth:issuer=base64(issuer)&audience=base64(audience)
  if (!info.api_key.empty()) {
    std::string credential_id("apiKey:");
    credential_id += info.api_key.ToString();
    (*labels)[l.name] = credential_id;
  } else if (!info.auth_issuer.empty()) {
    // If auth is used, auth_issuer should NOT be empty since it is required.
    char* base64_issuer = auth::esp_base64_encode(
        info.auth_issuer.data(), info.auth_issuer.size(), true /* url_safe */,
        false /* multiline */, false /* padding */);
    if (base64_issuer == nullptr) {
      return Status(Code::INTERNAL, "Failed to allocate memory.");
    }
    std::string credential_id("jwtAuth:issuer=");
    credential_id += base64_issuer;
    auth::esp_grpc_free(base64_issuer);

    // auth audience is optional.
    if (!info.auth_audience.empty()) {
      char* base64_audience = auth::esp_base64_encode(
          info.auth_audience.data(), info.auth_audience.size(),
          true /* url_safe */, false /* multiline */, false /* padding */);
      if (base64_audience == nullptr) {
        return Status(Code::INTERNAL, "Failed to allocate memory.");
      }

      credential_id += "&audience=";
      credential_id += base64_audience;
      auth::esp_grpc_free(base64_audience);
    }
    (*labels)[l.name] = credential_id;
  }
  return Status::OK;
}

const char* error_types[10] = {"0xx", "1xx", "2xx", "3xx", "4xx",
                               "5xx", "6xx", "7xx", "8xx", "9xx"};

// /error_type
Status set_error_type(const SupportedLabel& l, const ReportRequestInfo& info,
                      Map<std::string, std::string>* labels) {
  if (info.response_code >= 400) {
    int code = (info.response_code / 100) % 10;
    if (error_types[code]) {
      (*labels)[l.name] = error_types[code];
    }
  }
  return Status::OK;
}

// /protocol
Status set_protocol(const SupportedLabel& l, const ReportRequestInfo& info,
                    Map<std::string, std::string>* labels) {
  (*labels)[l.name] = protocol::ToString(info.protocol);
  return Status::OK;
}

// /referer
Status set_referer(const SupportedLabel& l, const ReportRequestInfo& info,
                   Map<std::string, std::string>* labels) {
  if (!info.referer.empty()) {
    (*labels)[l.name] = info.referer;
  }
  return Status::OK;
}

// /response_code
Status set_response_code(const SupportedLabel& l, const ReportRequestInfo& info,
                         Map<std::string, std::string>* labels) {
  char response_code_buf[20];
  snprintf(response_code_buf, sizeof(response_code_buf), "%d",
           info.response_code);
  (*labels)[l.name] = response_code_buf;
  return Status::OK;
}

// /response_code_class
Status set_response_code_class(const SupportedLabel& l,
                               const ReportRequestInfo& info,
                               Map<std::string, std::string>* labels) {
  (*labels)[l.name] = error_types[(info.response_code / 100) % 10];
  return Status::OK;
}

// /status_code
Status set_status_code(const SupportedLabel& l, const ReportRequestInfo& info,
                       Map<std::string, std::string>* labels) {
  char status_code_buf[20];
  snprintf(status_code_buf, sizeof(status_code_buf), "%d",
           info.status.CanonicalCode());
  (*labels)[l.name] = status_code_buf;
  return Status::OK;
}

// cloud.googleapis.com/location
Status set_location(const SupportedLabel& l, const ReportRequestInfo& info,
                    Map<std::string, std::string>* labels) {
  if (!info.location.empty()) {
    (*labels)[l.name] = info.location;
  }
  return Status::OK;
}

// serviceruntime.googleapis.com/api_method
Status set_api_method(const SupportedLabel& l, const ReportRequestInfo& info,
                      Map<std::string, std::string>* labels) {
  if (!info.api_method.empty()) {
    (*labels)[l.name] = info.api_method;
  }
  return Status::OK;
}

// serviceruntime.googleapis.com/api_version
Status set_api_version(const SupportedLabel& l, const ReportRequestInfo& info,
                       Map<std::string, std::string>* labels) {
  if (!info.api_name.empty()) {
    (*labels)[l.name] = info.api_name;
  }
  return Status::OK;
}

// servicecontrol.googleapis.com/platform
Status set_platform(const SupportedLabel& l, const ReportRequestInfo& info,
                    Map<std::string, std::string>* labels) {
  (*labels)[l.name] = compute_platform::ToString(info.compute_platform);
  return Status::OK;
}

// servicecontrol.googleapis.com/service_agent
Status set_service_agent(const SupportedLabel& l, const ReportRequestInfo& info,
                         Map<std::string, std::string>* labels) {
  (*labels)[l.name] = kServiceAgent;
  return Status::OK;
}

// serviceruntime.googleapis.com/user_agent
Status set_user_agent(const SupportedLabel& l, const ReportRequestInfo& info,
                      Map<std::string, std::string>* labels) {
  (*labels)[l.name] = kUserAgent;
  return Status::OK;
}

const SupportedLabel supported_labels[] = {
    {
        "/credential_id", ::google::api::LabelDescriptor_ValueType_STRING,
        SupportedLabel::USER, set_credential_id,
    },
    {
        "/end_user", ::google::api::LabelDescriptor_ValueType_STRING,
        SupportedLabel::USER, nullptr,
    },
    {
        "/end_user_country", ::google::api::LabelDescriptor_ValueType_STRING,
        SupportedLabel::USER, nullptr,
    },
    {
        "/error_type", ::google::api::LabelDescriptor_ValueType_STRING,
        SupportedLabel::USER, set_error_type,
    },
    {
        "/protocol", ::google::api::LabelDescriptor::STRING,
        SupportedLabel::USER, set_protocol,
    },
    {
        "/referer", ::google::api::LabelDescriptor_ValueType_STRING,
        SupportedLabel::USER, set_referer,
    },
    {
        "/response_code", ::google::api::LabelDescriptor_ValueType_STRING,
        SupportedLabel::USER, set_response_code,
    },
    {
        "/response_code_class", ::google::api::LabelDescriptor::STRING,
        SupportedLabel::USER, set_response_code_class,
    },
    {
        "/status_code", ::google::api::LabelDescriptor_ValueType_STRING,
        SupportedLabel::USER, set_status_code,
    },
    {
        "appengine.googleapis.com/clone_id",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::USER,
        nullptr,
    },
    {
        "appengine.googleapis.com/module_id",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::USER,
        nullptr,
    },
    {
        "appengine.googleapis.com/replica_index",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::USER,
        nullptr,
    },
    {
        "appengine.googleapis.com/version_id",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::USER,
        nullptr,
    },
    {
        "cloud.googleapis.com/location",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::SYSTEM,
        set_location,
    },
    {
        "cloud.googleapis.com/project",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::SYSTEM,
        nullptr,
    },
    {
        "cloud.googleapis.com/region",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::SYSTEM,
        nullptr,
    },
    {
        "cloud.googleapis.com/resource_id",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::USER,
        nullptr,
    },
    {
        "cloud.googleapis.com/resource_type",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::USER,
        nullptr,
    },
    {
        "cloud.googleapis.com/service",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::SYSTEM,
        nullptr,
    },
    {
        "cloud.googleapis.com/zone",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::SYSTEM,
        nullptr,
    },
    {
        "cloud.googleapis.com/uid",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::SYSTEM,
        nullptr,
    },
    {
        "serviceruntime.googleapis.com/api_method",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::USER,
        set_api_method,
    },
    {
        "serviceruntime.googleapis.com/api_version",
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::USER,
        set_api_version,
    },
    {
        kServiceControlCallerIp,
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::SYSTEM,
        nullptr,
    },
    {
        kServiceControlReferer, ::google::api::LabelDescriptor_ValueType_STRING,
        SupportedLabel::SYSTEM, nullptr,
    },
    {
        kServiceControlServiceAgent,
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::SYSTEM,
        set_service_agent,
    },
    {
        kServiceControlUserAgent,
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::SYSTEM,
        set_user_agent,
    },
    {
        kServiceControlPlatform,
        ::google::api::LabelDescriptor_ValueType_STRING, SupportedLabel::SYSTEM,
        set_platform,
    },
};

const int supported_labels_count =
    sizeof(supported_labels) / sizeof(supported_labels[0]);

// Supported intrinsic labels:
// "servicecontrol.googleapis.com/operation_name": Operation.operation_name
// "servicecontrol.googleapis.com/consumer_id": Operation.consumer_id

// Unsupported service control labels:
// "servicecontrol.googleapis.com/android_package_name"
// "servicecontrol.googleapis.com/android_cert_fingerprint"
// "servicecontrol.googleapis.com/ios_bundle_id"
// "servicecontrol.googleapis.com/credential_project_number"

// Define Service Control constant strings
const char kConsumerIdApiKey[] = "api_key:";
const char kConsumerIdProject[] = "project:";

// Following names for for Log struct_playload field names:
const char kLogFieldNameTimestamp[] = "timestamp";
const char kLogFieldNameApiName[] = "api_name";
const char kLogFieldNameApiMethod[] = "api_method";
const char kLogFieldNameApiKey[] = "api_key";
const char kLogFieldNameProducerProjectId[] = "producer_project_id";
const char kLogFieldNameReferer[] = "referer";
const char kLogFieldNameLocation[] = "location";
const char kLogFieldNameRequestSize[] = "request_size";
const char kLogFieldNameResponseSize[] = "response_size";
const char kLogFieldNameHttpMethod[] = "http_method";
const char kLogFieldNameHttpResponseCode[] = "http_response_code";
const char kLogFieldNameLogMessage[] = "log_message";
const char kLogFieldNameRequestLatency[] = "request_latency_in_ms";
const char kLogFieldNameUrl[] = "url";
const char kLogFieldNameErrorCause[] = "error_cause";

Timestamp GetCurrentTimestamp() {
  struct timeval tv;
  struct timezone tz;
  gettimeofday(&tv, &tz);
  Timestamp current_time;
  current_time.set_seconds(tv.tv_sec);
  current_time.set_nanos(tv.tv_usec * 1000);
  return current_time;
}

Status VerifyRequiredCheckFields(const OperationInfo& info) {
  if (info.service_name.empty()) {
    return Status(Code::INVALID_ARGUMENT, "service_name is required.",
                  Status::SERVICE_CONTROL);
  }
  if (info.operation_id.empty()) {
    return Status(Code::INVALID_ARGUMENT, "operation_id is required.",
                  Status::SERVICE_CONTROL);
  }
  if (info.operation_name.empty()) {
    return Status(Code::INVALID_ARGUMENT, "operation_name is required.",
                  Status::SERVICE_CONTROL);
  }
  return Status::OK;
}

Status VerifyRequiredReportFields(const OperationInfo& info) {
  if (info.service_name.empty()) {
    return Status(Code::INVALID_ARGUMENT, "service_name is required.",
                  Status::SERVICE_CONTROL);
  }
  return Status::OK;
}

void SetOperationCommonFields(const OperationInfo& info,
                              const Timestamp& current_time, Operation* op) {
  if (!info.operation_id.empty()) {
    op->set_operation_id(info.operation_id);
  }
  if (!info.operation_name.empty()) {
    op->set_operation_name(info.operation_name);
  }
  // TODO: info.producer_project_id currently refers to producer project
  // id. Needs to clean this up.
  // Sets api_key for consumer_id if it exists and is valid. Otherwise use
  // info.producer_project_id as the consumer_id.
  // info.is_api_key_valid is always true for the check request. If the check
  // request failed with an invalid api key error, info.is_api_key_valid will
  // be set false.
  if (!info.api_key.empty() && info.is_api_key_valid) {
    op->set_consumer_id(std::string(kConsumerIdApiKey) +
                        std::string(info.api_key));
  } else if (!info.producer_project_id.empty()) {
    op->set_consumer_id(std::string(kConsumerIdProject) +
                        std::string(info.producer_project_id));
  }
  *op->mutable_start_time() = current_time;
  *op->mutable_end_time() = current_time;
}

Status CreateErrorStatus(int code, StringPiece message) {
  return Status((Code)code, message, Status::SERVICE_CONTROL);
}

void FillLogEntry(const ReportRequestInfo& info, const std::string& name,
                  const Timestamp& current_time, LogEntry* log_entry) {
  log_entry->set_name(name);
  *log_entry->mutable_timestamp() = current_time;
  auto severity = (info.response_code >= 400) ? google::logging::type::ERROR
                                              : google::logging::type::INFO;
  log_entry->set_severity(severity);

  auto* fields = log_entry->mutable_struct_payload()->mutable_fields();
  (*fields)[kLogFieldNameTimestamp].set_number_value(
      (double)current_time.seconds() +
      (double)current_time.nanos() / (double)1000000000.0);
  if (!info.producer_project_id.empty()) {
    (*fields)[kLogFieldNameProducerProjectId].set_string_value(
        info.producer_project_id);
  }
  if (!info.api_key.empty()) {
    (*fields)[kLogFieldNameApiKey].set_string_value(info.api_key);
  }
  if (!info.referer.empty()) {
    (*fields)[kLogFieldNameReferer].set_string_value(info.referer);
  }
  if (!info.api_name.empty()) {
    (*fields)[kLogFieldNameApiName].set_string_value(info.api_name);
  }
  if (!info.url.empty()) {
    (*fields)[kLogFieldNameUrl].set_string_value(info.url);
  }
  if (!info.api_method.empty()) {
    (*fields)[kLogFieldNameApiMethod].set_string_value(info.api_method);
  }
  if (!info.location.empty()) {
    (*fields)[kLogFieldNameLocation].set_string_value(info.location);
  }
  if (!info.log_message.empty()) {
    (*fields)[kLogFieldNameLogMessage].set_string_value(info.log_message);
  }

  (*fields)[kLogFieldNameHttpResponseCode].set_number_value(info.response_code);

  if (info.request_size >= 0) {
    (*fields)[kLogFieldNameRequestSize].set_number_value(info.request_size);
  }
  if (info.response_size >= 0) {
    (*fields)[kLogFieldNameResponseSize].set_number_value(info.response_size);
  }
  if (info.latency.request_time_ms >= 0) {
    (*fields)[kLogFieldNameRequestLatency].set_number_value(
        info.latency.request_time_ms);
  }
  if (!info.method.empty()) {
    (*fields)[kLogFieldNameHttpMethod].set_string_value(info.method);
  }
  if (info.response_code >= 400) {
    (*fields)[kLogFieldNameErrorCause].set_string_value(
        info.status.GetErrorCauseString());
  }
}

template <class Element>
std::vector<const Element*> FilterPointers(
    const Element* first, const Element* last,
    std::function<bool(const Element*)> pred) {
  std::vector<const Element*> filtered;
  while (first < last) {
    if (pred(first)) {
      filtered.push_back(first);
    }
    first++;
  }
  return filtered;
}

}  // namespace

Proto::Proto(const std::set<std::string>& logs)
    : logs_(logs.begin(), logs.end()),
      metrics_(FilterPointers<SupportedMetric>(
          supported_metrics, supported_metrics + supported_metrics_count,
          [](const struct SupportedMetric* m) { return m->set != nullptr; })),
      labels_(FilterPointers<SupportedLabel>(
          supported_labels, supported_labels + supported_labels_count,
          [](const struct SupportedLabel* l) { return l->set != nullptr; })) {}

Proto::Proto(const std::set<std::string>& logs,
             const std::set<std::string>& metrics,
             const std::set<std::string>& labels)
    : logs_(logs.begin(), logs.end()),
      metrics_(FilterPointers<SupportedMetric>(
          supported_metrics, supported_metrics + supported_metrics_count,
          [&metrics](const struct SupportedMetric* m) {
            return m->set && metrics.find(m->name) != metrics.end();
          })),
      labels_(FilterPointers<SupportedLabel>(
          supported_labels, supported_labels + supported_labels_count,
          [&labels](const struct SupportedLabel* l) {
            return l->set && (l->kind == SupportedLabel::SYSTEM ||
                              labels.find(l->name) != labels.end());
          })) {}

Status Proto::FillCheckRequest(const CheckRequestInfo& info,
                               CheckRequest* request) {
  Status status = VerifyRequiredCheckFields(info);
  if (!status.ok()) {
    return status;
  }
  request->set_service_name(info.service_name);

  Timestamp current_time = GetCurrentTimestamp();
  Operation* op = request->mutable_operation();
  SetOperationCommonFields(info, current_time, op);

  auto* labels = op->mutable_labels();
  if (!info.client_ip.empty()) {
    (*labels)[kServiceControlCallerIp] = info.client_ip;
  }
  if (!info.referer.empty()) {
    (*labels)[kServiceControlReferer] = info.referer;
  }
  (*labels)[kServiceControlUserAgent] = kUserAgent;
  (*labels)[kServiceControlServiceAgent] = kServiceAgent;
  return Status::OK;
}

Status Proto::FillReportRequest(const ReportRequestInfo& info,
                                ReportRequest* request) {
  Status status = VerifyRequiredReportFields(info);
  if (!status.ok()) {
    return status;
  }
  request->set_service_name(info.service_name);

  Timestamp current_time = GetCurrentTimestamp();
  Operation* op = request->add_operations();
  SetOperationCommonFields(info, current_time, op);

  // Only populate metrics if we can associate them with a method/operation.
  if (!info.operation_id.empty() && !info.operation_name.empty()) {
    Map<std::string, std::string>* labels = op->mutable_labels();
    // Set all labels.
    for (auto it = labels_.begin(), end = labels_.end(); it != end; it++) {
      const SupportedLabel* l = *it;
      if (l->set) {
        status = (l->set)(*l, info, labels);
        if (!status.ok()) return status;
      }
    }

    // Populate all metrics.
    for (auto it = metrics_.begin(), end = metrics_.end(); it != end; it++) {
      const SupportedMetric* m = *it;
      if (m->set) {
        status = (m->set)(*m, info, op);
        if (!status.ok()) return status;
      }
    }
  }

  // Fill log entries.
  for (auto it = logs_.begin(), end = logs_.end(); it != end; it++) {
    FillLogEntry(info, *it, current_time, op->add_log_entries());
  }
  return Status::OK;
}

Status Proto::ConvertCheckResponse(const CheckResponse& check_response,
                                   const std::string& project_id,
                                   CheckResponseInfo* check_response_info) {
  if (check_response_info) check_response_info->is_api_key_valid = true;
  if (check_response.check_errors().size() == 0) {
    return Status::OK;
  }
  // We only check the first error for now, same as ESF now.
  const CheckError& error = check_response.check_errors(0);
  switch (error.code()) {
    case CheckError::NOT_FOUND:  // The consumer's project id is not found.
      return CreateErrorStatus(
          Code::INVALID_ARGUMENT,
          "Client project not found. Please pass a valid project.");
    case CheckError::API_KEY_NOT_FOUND:
      if (check_response_info) check_response_info->is_api_key_valid = false;
      return CreateErrorStatus(
          Code::INVALID_ARGUMENT,
          "API Key not found. Please pass a valid API key.");
    case CheckError::API_KEY_EXPIRED:
      if (check_response_info) check_response_info->is_api_key_valid = false;
      return CreateErrorStatus(Code::INVALID_ARGUMENT,
                               "API key expired. Please renew the API key.");
    case CheckError::API_KEY_INVALID:
      if (check_response_info) check_response_info->is_api_key_valid = false;
      return CreateErrorStatus(
          Code::INVALID_ARGUMENT,
          "API key not valid. Please pass a valid API key.");
    case CheckError::SERVICE_NOT_ACTIVATED:
      return CreateErrorStatus(Code::PERMISSION_DENIED,
                               error.detail() +
                                   " Please enable the API for project " +
                                   project_id + ".");
    case CheckError::PERMISSION_DENIED:
      return CreateErrorStatus(
          Code::PERMISSION_DENIED,
          std::string("Permission denied: ") + error.detail());
    case CheckError::IP_ADDRESS_BLOCKED:
      return CreateErrorStatus(Code::PERMISSION_DENIED, error.detail());
    case CheckError::REFERER_BLOCKED:
      return CreateErrorStatus(Code::PERMISSION_DENIED, error.detail());
    case CheckError::CLIENT_APP_BLOCKED:
      return CreateErrorStatus(Code::PERMISSION_DENIED, error.detail());
    case CheckError::PROJECT_DELETED:
      return CreateErrorStatus(
          Code::PERMISSION_DENIED,
          std::string("Project ") + project_id + " has been deleted.");
    case CheckError::PROJECT_INVALID:
      return CreateErrorStatus(
          Code::INVALID_ARGUMENT,
          "Client project not valid. Please pass a valid project.");
    case CheckError::VISIBILITY_DENIED:
      return CreateErrorStatus(Code::PERMISSION_DENIED,
                               std::string("Project ") + project_id +
                                   " has no visibility access to the service.");
    case CheckError::BILLING_DISABLED:
      return CreateErrorStatus(Code::PERMISSION_DENIED,
                               std::string("Project ") + project_id +
                                   " has billing disabled. Please enable it.");
    case CheckError::NAMESPACE_LOOKUP_UNAVAILABLE:
    case CheckError::SERVICE_STATUS_UNAVAILABLE:
    case CheckError::BILLING_STATUS_UNAVAILABLE:
    case CheckError::QUOTA_CHECK_UNAVAILABLE:
      return Status::OK;  // Fail open for internal server errors
    default:
      return CreateErrorStatus(
          Code::INTERNAL,
          std::string("Request blocked due to unsupported BlockReason: ") +
              error.detail());
  }
  return Status::OK;
}

bool Proto::IsMetricSupported(const ::google::api::MetricDescriptor& metric) {
  for (int i = 0; i < supported_metrics_count; i++) {
    const SupportedMetric& m = supported_metrics[i];
    if (metric.name() == m.name && metric.metric_kind() == m.metric_kind &&
        metric.value_type() == m.value_type) {
      return true;
    }
  }
  return false;
}

bool Proto::IsLabelSupported(const ::google::api::LabelDescriptor& label) {
  for (int i = 0; i < supported_labels_count; i++) {
    const SupportedLabel& l = supported_labels[i];
    if (label.key() == l.name && label.value_type() == l.value_type) {
      return true;
    }
  }
  return false;
}

}  // namespace service_control
}  // namespace api_manager
}  // namespace google
