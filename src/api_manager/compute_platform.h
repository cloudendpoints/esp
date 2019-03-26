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
#ifndef API_MANAGER_COMPUTE_PLATFORM_H_
#define API_MANAGER_COMPUTE_PLATFORM_H_

#include <string>

namespace google {
namespace api_manager {

struct ComputePlatform {
  static const std::string kGaeFlex;
  static const std::string kGce;
  static const std::string kGke;
  static const std::string kUnknown;
};

}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_COMPUTE_PLATFORM_H_
