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
#include "src/api_manager/cloud_trace/sampler.h"

namespace google {
namespace api_manager {
namespace cloud_trace {

Sampler::Sampler(double qps) {
  if (qps == 0.0) {
    is_disabled_ = true;
  } else {
    duration_ = 1.0 / qps;
    is_disabled_ = false;
  }
}

bool Sampler::On() {
  if (is_disabled_) {
    return false;
  }
  auto now = std::chrono::system_clock::now();
  std::chrono::duration<double> diff = now - previous_;
  if (diff.count() > duration_) {
    previous_ = now;
    return true;
  } else {
    return false;
  }
};

void Sampler::Refresh() {
  if (is_disabled_) {
    return;
  }
  previous_ = std::chrono::system_clock::now();
}

}  // namespace cloud_trace
}  // namespace api_manager
}  // namespace google
