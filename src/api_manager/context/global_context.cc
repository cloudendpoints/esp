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
#include "src/api_manager/context/global_context.h"
#include "src/api_manager/config.h"
#include "src/api_manager/service_control/aggregated.h"

namespace google {
namespace api_manager {
namespace context {

namespace {

// Default Cloud Trace URL. Points to prod Cloud Trace.
const char kCloudTraceUrl[] = "https://cloudtrace.googleapis.com";

// Default maximum time to aggregate traces.
const int kDefaultAggregateTimeMillisec = 1000;

// Default maximum amount of traces to aggregate. The amount should ensure
// the http request payload with the aggregated traces not reaching MB in size.
const int kDefaultTraceCacheMaxSize = 100;

// Default trace sample rate, in QPS.
const double kDefaultTraceSampleQps = 0.1;

// The time window to send intermediate report for Grpc streaming (second).
// Default to 10s.
const int kIntermediateReportInterval = 10;

}  // namespace

GlobalContext::GlobalContext(std::unique_ptr<ApiManagerEnvInterface> env,
                             const std::string& server_config)
    : env_(std::move(env)),
      service_account_token_(env_.get()),
      is_auth_force_disabled_(false),
      disable_log_status_(false),
      always_print_primitive_fields_(false),
      intermediate_report_interval_(kIntermediateReportInterval),
      platform_(compute_platform::UNKNOWN) {
  // Need to load server config first.
  server_config_ = Config::LoadServerConfig(env_.get(), server_config);

  cloud_trace_aggregator_ = CreateCloudTraceAggregator();

  if (server_config_) {
    if (server_config_->has_metadata_server_config() &&
        server_config_->metadata_server_config().enabled()) {
      metadata_server_ = server_config_->metadata_server_config().url();
    }

    rollout_strategy_ = server_config_->rollout_strategy();

    service_account_token_.SetClientAuthSecret(
        server_config_->google_authentication_secret());

    is_auth_force_disabled_ =
        server_config_->has_api_authentication_config() &&
        server_config_->api_authentication_config().force_disable();

    // Check server_config override.
    if (server_config_->has_service_control_config() &&
        server_config_->service_control_config()
            .intermediate_report_min_interval()) {
      intermediate_report_interval_ = server_config_->service_control_config()
                                          .intermediate_report_min_interval();
    }

    if (server_config_->has_metadata_attributes()) {
      const auto& metadata = server_config_->metadata_attributes();
      if (!metadata.gae_server_software().empty()) {
        platform_ = compute_platform::GAE_FLEX;
      } else if (!metadata.kube_env().empty()) {
        platform_ = compute_platform::GKE;
      } else {
        platform_ = compute_platform::GCE;
      }
      location_ = metadata.zone();
      project_id_ = metadata.project_id();

      if (metadata.has_access_token()) {
        const auto& token = metadata.access_token();
        service_account_token_.set_state(auth::ServiceAccountToken::FETCHED);
        service_account_token_.set_access_token(token.access_token(),
                                                token.expires_in() - 50);
      }
    }

    if (server_config_->has_experimental()) {
      const auto& experimental = server_config_->experimental();
      disable_log_status_ = experimental.disable_log_status();
      always_print_primitive_fields_ =
          experimental.always_print_primitive_fields();
    }
  }
}

std::unique_ptr<cloud_trace::Aggregator>
GlobalContext::CreateCloudTraceAggregator() {
  // If force_disable is set in server config, completely disable tracing.
  if (server_config_ &&
      server_config_->cloud_tracing_config().force_disable()) {
    env()->LogInfo(
        "Cloud Trace is force disabled. There will be no trace written.");
    return std::unique_ptr<cloud_trace::Aggregator>();
  }

  std::string url = kCloudTraceUrl;
  int aggregate_time_millisec = kDefaultAggregateTimeMillisec;
  int cache_max_size = kDefaultTraceCacheMaxSize;
  double minimum_qps = kDefaultTraceSampleQps;
  if (server_config_ && server_config_->has_cloud_tracing_config()) {
    // If url_override is set in server config, use it to query Cloud Trace.
    const auto& tracing_config = server_config_->cloud_tracing_config();
    if (!tracing_config.url_override().empty()) {
      url = tracing_config.url_override();
    }

    // If aggregation config is set, take the values from it.
    if (tracing_config.has_aggregation_config()) {
      aggregate_time_millisec =
          tracing_config.aggregation_config().time_millisec();
      cache_max_size = tracing_config.aggregation_config().cache_max_size();
    }

    // If sampling config is set, take the values from it.
    if (tracing_config.has_samling_config()) {
      minimum_qps = tracing_config.samling_config().minimum_qps();
    }
  }

  return std::unique_ptr<cloud_trace::Aggregator>(new cloud_trace::Aggregator(
      &service_account_token_, url, aggregate_time_millisec, cache_max_size,
      minimum_qps, env_.get()));
}

auth::ServiceAccountToken* GlobalContext::GetInstanceIdentityToken(
    const std::string& audience) {
  auto token_info = instance_identity_token_map_.find(audience);
  if (token_info != std::end(instance_identity_token_map_)) {
    return token_info->second.get();
  }

  auto token = std::unique_ptr<auth::ServiceAccountToken>(
      new auth::ServiceAccountToken(env_.get()));
  auto raw_token = token.get();
  instance_identity_token_map_.emplace(audience, std::move(token));
  return raw_token;
}

}  // namespace context
}  // namespace api_manager
}  // namespace google
