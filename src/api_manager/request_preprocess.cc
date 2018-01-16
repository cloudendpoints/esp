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
#include <iostream>

#include "google/protobuf/stubs/status.h"
#include "src/api_manager/cloud_trace/cloud_trace.h"
#include "src/api_manager/quota_control.h"

using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace api_manager {

namespace {
// The header key to send endpoint api user info.
const char kEndpointApiUserInfo[] = "X-Endpoint-API-UserInfo";
}

void RequestValidator(std::shared_ptr<context::RequestContext> context,
                  std::function<void(utils::Status)> continuation) {
  // Clear the X-Endpoint-API-UserInfo from the request headers
  context->request()->AddHeaderToBackend(kEndpointApiUserInfo, "");

  continuation(Status::OK);
}

}  // namespace api_manager
}  // namespace google
