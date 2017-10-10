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
#include "src/api_manager/api_base_path.h"

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {

void ApiBasePath(std::shared_ptr<context::RequestContext> context,
                 std::function<void(Status status)> continuation) {
  std::shared_ptr<cloud_trace::CloudTraceSpan> trace_span(
      CreateSpan(context->cloud_trace(), "ApiBasePath"));

  auto server_config =
      context->service_context()->global_context()->server_config();

  if (server_config->has_api_service_config() == false ||
      server_config->api_service_config().base_path().length() == 0) {
    TRACE(trace_span) << "ApiBasePath handling is not needed";
    continuation(Status::OK);
    return;
  }

  context->request()->SetRequestPath(
      Config::GetStripedPath(context->request()->GetUnparsedRequestPath(),
                             server_config->api_service_config().base_path()));

  continuation(Status::OK);
}

}  // namespace api_manager
}  // namespace google
