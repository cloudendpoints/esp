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
#ifndef API_MANAGER_CLOUD_TRACE_SAMPLER_H_
#define API_MANAGER_CLOUD_TRACE_SAMPLER_H_

#include <chrono>

namespace google {
namespace api_manager {
namespace cloud_trace {

// A helper class to determine if trace should be enabled for a request.
// A Sampler instance is put into the Aggregator class.
// Trace is triggered if the time interval between the request time and the
// previous trace enabled request is bigger than a threshold.
// The threshold is calculated from the qps.
class Sampler {
 public:
  Sampler(double qps);

  // Returns whether trace should be turned on for this request.
  bool On();

  // Refresh the previous timestamp to the current time.
  void Refresh();

 private:
  bool is_disabled_;
  std::chrono::time_point<std::chrono::system_clock> previous_;
  double duration_;
};

}  // namespace cloud_trace
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_CLOUD_TRACE_SAMPLER_H_
