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
// includes should be ordered. This seems like a bug in clang-format?
#include "src/api_manager/check_service_control.h"
#include "google/protobuf/stubs/status.h"
#include "src/api_manager/cloud_trace/cloud_trace.h"

using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace api_manager {

namespace {

const std::string kConsumerProjecId = "X-Endpoint-API-Project-ID";
}

void CheckServiceControl(std::shared_ptr<context::RequestContext> context,
                         std::function<void(Status status)> continuation) {
  // If the method is not configured from the service config.
  // or if not need to check service control, skip it.
  if (!context->method()) {
    if (context->GetRequestHTTPMethodWithOverride() == "OPTIONS") {
      continuation(Status(Code::PERMISSION_DENIED,
                          "The service does not allow CORS traffic.",
                          Status::SERVICE_CONTROL));
    } else {
      continuation(Status(Code::NOT_FOUND, "Method does not exist.",
                          Status::SERVICE_CONTROL));
    }
    return;
  } else if (!context->service_context()->service_control() ||
             context->method()->skip_service_control()) {
    continuation(Status::OK);
    return;
  }

  if (context->api_key().empty()) {
    if (context->method()->allow_unregistered_calls()) {
      // Not need to call Check.
      continuation(Status::OK);
      return;
    }

    continuation(
        Status(Code::UNAUTHENTICATED,
               "Method doesn't allow unregistered callers (callers without "
               "established identity). Please use API Key or other form of "
               "API consumer identity to call this API.",
               Status::SERVICE_CONTROL));
    return;
  }

  std::shared_ptr<cloud_trace::CloudTraceSpan> trace_span(
      CreateSpan(context->cloud_trace(), "CheckServiceControl"));

  service_control::CheckRequestInfo info;
  context->FillCheckRequestInfo(&info);
  context->service_context()->service_control()->Check(
      info, trace_span.get(),
      [context, continuation, trace_span](
          Status status, const service_control::CheckResponseInfo &info) {
        TRACE(trace_span) << "Check service control request returned with "
                          << "status " << status.ToString();
        // info is valid regardless status.
        context->set_check_response_info(info);

        // update consumer_project_id to service context
        if (!info.consumer_project_id.empty()) {
          context->request()->AddHeaderToBackend(kConsumerProjecId,
                                                 info.consumer_project_id);
        }

        continuation(status);
      });
}

}  // namespace api_manager
}  // namespace google
