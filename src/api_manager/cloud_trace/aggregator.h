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
//
#ifndef API_MANAGER_CLOUD_TRACE_AGGREGATOR_H_
#define API_MANAGER_CLOUD_TRACE_AGGREGATOR_H_

#include "google/devtools/cloudtrace/v1/trace.pb.h"
#include "include/api_manager/env_interface.h"
#include "include/api_manager/periodic_timer.h"
#include "src/api_manager/auth/service_account_token.h"
#include "src/api_manager/cloud_trace/sampler.h"

namespace google {
namespace api_manager {
namespace cloud_trace {

// TODO: The Aggregator class is not thread safe.
// TODO: simplify class naming in this file.
// Stores cloud trace configurations shared within the job. There should be
// only one such instance. The instance is put in service_context.
class Aggregator final {
 public:
  Aggregator(auth::ServiceAccountToken *sa_token,
             const std::string &cloud_trace_address,
             int aggregate_time_millisec, int cache_max_size,
             double minimum_qps, ApiManagerEnvInterface *env);

  ~Aggregator();

  // Initializes the aggregator by setting up a periodic timer. At each timer
  // invocation traces aggregated are sent to Cloud Trace API
  void Init();

  // Flush traces cached and clear the traces_ proto.
  void SendAndClearTraces();

  // Append a Trace to traces_, the appended trace may not be sent at the time
  // of this function call.
  void AppendTrace(google::devtools::cloudtrace::v1::Trace *trace);

  // Sets the producer project id
  void SetProjectId(const std::string &project_id) { project_id_ = project_id; }

  // Get the sampler.
  Sampler &sampler() { return sampler_; }

 private:
  // ServiceAccountToken object to get auth tokens for Cloud Trace API.
  auth::ServiceAccountToken *sa_token_;

  // Address for Cloud Trace API
  std::string cloud_trace_address_;

  // The maximum time to hold a trace before sent to Cloud Trace.
  int aggregate_time_millisec_;

  // The maximum number of traces that can be cached.
  int cache_max_size_;

  // Traces protobuf to hold a list of Trace obejcts.
  std::unique_ptr<google::devtools::cloudtrace::v1::Traces> traces_;

  // The producer project id.
  std::string project_id_;

  // ApiManager Env used to set up periodic timer.
  ApiManagerEnvInterface *env_;

  // Timer to trigger flush trace.
  std::unique_ptr<google::api_manager::PeriodicTimer> timer_;

  // Sampler object to help determine if trace should be enabled for a request.
  Sampler sampler_;
};

}  // namespace cloud_trace
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_CLOUD_TRACE_AGGREGATOR_H_
