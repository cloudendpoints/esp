// Copyright 2017 Google Inc. All Rights Reserved.
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
#ifndef API_MANAGER_QUOTA_CONTROL_H_
#define API_MANAGER_QUOTA_CONTROL_H_

#include "contrib/endpoints/include/api_manager/utils/status.h"
#include "contrib/endpoints/src/api_manager/context/request_context.h"

namespace google {
namespace api_manager {

// Call service control quota.
void QuotaControl(std::shared_ptr<context::RequestContext>,
                  std::function<void(utils::Status)>);

}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_QUOTA_CONTROL_H_
