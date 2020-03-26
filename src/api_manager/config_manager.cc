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
#include "src/api_manager/config_manager.h"
#include "src/api_manager/fetch_metadata.h"
#include "src/api_manager/utils/marshalling.h"

namespace google {
namespace api_manager {

namespace {
// Default fetch throttle window in seconds
const int kFetchThrottleWindowInS = 300;

const char kRolloutStrategyManaged[] = "managed";

// The default periodical interval to detect rollout changes. Unit: seconds.
const int kDetectRolloutChangeIntervalInS = 60;

}  // namespace

ConfigManager::ConfigManager(
    std::shared_ptr<context::GlobalContext> global_context,
    RolloutApplyFunction rollout_apply_function,
    std::function<void()> detect_rollout_func)
    : global_context_(global_context),
      rollout_apply_function_(rollout_apply_function),
      fetch_throttle_window_in_s_(kFetchThrottleWindowInS),
      detect_rollout_func_(detect_rollout_func) {
  int detect_rollout_interval_s = kDetectRolloutChangeIntervalInS;
  if (global_context_->server_config() &&
      global_context_->server_config()->has_service_management_config()) {
    const auto& cfg =
        global_context_->server_config()->service_management_config();
    // update fetch_throttle_window
    if (cfg.fetch_throttle_window_s() > 0) {
      fetch_throttle_window_in_s_ = cfg.fetch_throttle_window_s();
    }
    if (cfg.detect_rollout_interval_s() > 0) {
      detect_rollout_interval_s = cfg.detect_rollout_interval_s();
    }
  }
  static std::random_device random_device;
  random_generator_.seed(random_device());

  // throttle in milliseconds
  random_dist_.reset(new std::uniform_int_distribution<int>(
      0, fetch_throttle_window_in_s_ * 1000));

  service_management_fetch_.reset(new ServiceManagementFetch(global_context));

  if (detect_rollout_func_) {
    detect_rollout_change_timer_ = global_context_->env()->StartPeriodicTimer(
        std::chrono::seconds(detect_rollout_interval_s),
        [this]() { OnDetectRolloutChangeTimer(); });
  }
}

ConfigManager::~ConfigManager() {
  if (fetch_timer_) {
    fetch_timer_->Stop();
  }
  if (detect_rollout_change_timer_) {
    detect_rollout_change_timer_->Stop();
  }
};

void ConfigManager::SetLatestRolloutId(
    const std::string& latest_rollout_id,
    std::chrono::time_point<std::chrono::system_clock> now) {
  if (latest_rollout_id == current_rollout_id_) {
    return;
  }

  // Last timer is not fired yet,  or is too close to last fetch time
  if ((fetch_timer_ && !fetch_timer_->IsStopped()) || now < next_window_time_) {
    return;
  }

  auto throttled_time_in_ms = (*random_dist_)(random_generator_);
  global_context_->env()->LogInfo("Schedule a fetch timer in ms: " +
                                  std::to_string(throttled_time_in_ms));
  fetch_timer_ = global_context_->env()->StartPeriodicTimer(
      std::chrono::milliseconds(throttled_time_in_ms),
      [this]() { OnRolloutsRefreshTimer(); });
}

void ConfigManager::OnDetectRolloutChangeTimer() {
  global_context_->env()->LogDebug("Detect rollout change timer starts");
  std::string audience;
  GlobalFetchServiceAccountToken(
      global_context_, audience, nullptr, [this](utils::Status status) {
        if (!status.ok()) {
          global_context_->env()->LogError(
              "Fetch access token unexpected status: " + status.ToString());
          return;
        }
        detect_rollout_func_();
      });
}

void ConfigManager::OnRolloutsRefreshTimer() {
  global_context_->env()->LogInfo("Fetch timer starts");
  fetch_timer_->Stop();
  next_window_time_ = std::chrono::system_clock::now() +
                      std::chrono::seconds(fetch_throttle_window_in_s_);

  std::string audience;
  GlobalFetchServiceAccountToken(
      global_context_, audience, nullptr, [this](utils::Status status) {
        if (!status.ok()) {
          global_context_->env()->LogError("Unexpected status: " +
                                           status.ToString());
          return;
        }
        FetchRollouts();
      });
}

void ConfigManager::FetchRollouts() {
  remote_rollout_calls_++;
  service_management_fetch_->GetRollouts(
      [this](const utils::Status& status, std::string&& rollouts) {
        OnRolloutResponse(status, std::move(rollouts));
      });
}

void ConfigManager::OnRolloutResponse(const utils::Status& status,
                                      std::string&& rollouts) {
  if (!status.ok()) {
    global_context_->env()->LogError(
        std::string("Failed to download rollouts: ") + status.ToString() +
        ", Response body: " + rollouts);
    return;
  }

  ListServiceRolloutsResponse response;
  if (!utils::JsonToProto(rollouts, (::google::protobuf::Message*)&response)
           .ok()) {
    global_context_->env()->LogError(std::string("Invalid response: ") +
                                     status.ToString() +
                                     ", Response body: " + rollouts);
    return;
  }

  if (response.rollouts_size() == 0) {
    global_context_->env()->LogError("No active rollouts");
    return;
  }

  if (current_rollout_id_ == response.rollouts(0).rollout_id()) {
    return;
  }

  std::shared_ptr<ConfigsFetchInfo> config_fetch_info =
      std::make_shared<ConfigsFetchInfo>();

  config_fetch_info->rollout_id = response.rollouts(0).rollout_id();

  for (auto percentage :
       response.rollouts(0).traffic_percent_strategy().percentages()) {
    config_fetch_info->rollouts.push_back(
        {percentage.first, round(percentage.second)});
  }

  if (config_fetch_info->rollouts.size() == 0) {
    global_context_->env()->LogError("No active rollouts");
    return;
  }

  FetchConfigs(config_fetch_info);
}

// Fetch configs from rollouts. fetch_info has rollouts and fetched configs
void ConfigManager::FetchConfigs(
    std::shared_ptr<ConfigsFetchInfo> config_fetch_info) {
  for (auto rollout : config_fetch_info->rollouts) {
    std::string config_id = rollout.first;
    int percentage = rollout.second;
    service_management_fetch_->GetConfig(config_id, [this, config_id,
                                                     percentage,
                                                     config_fetch_info](
                                                        const utils::Status&
                                                            status,
                                                        std::string&& config) {
      if (status.ok()) {
        config_fetch_info->configs.push_back({std::move(config), percentage});
      } else {
        global_context_->env()->LogError(std::string(
            "Unable to download Service config for the config_id: " +
            config_id));
      }

      config_fetch_info->finished++;

      if (config_fetch_info->IsCompleted()) {
        if (config_fetch_info->IsRolloutsEmpty() ||
            config_fetch_info->IsConfigsEmpty() ||
            config_fetch_info->rollouts.size() !=
                config_fetch_info->configs.size()) {
          global_context_->env()->LogError(
              "Failed to download the service config");
          return;
        }

        // Update ApiManager
        rollout_apply_function_(utils::Status::OK,
                                std::move(config_fetch_info->configs));
        current_rollout_id_ = config_fetch_info->rollout_id;
      }
    });
  }
}

}  // namespace api_manager
}  // namespace google
