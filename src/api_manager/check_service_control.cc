/*
 * Copyright (C) Endpoints Server Proxy Authors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "src/api_manager/check_service_control.h"
#include "src/api_manager/cloud_trace/cloud_trace.h"

using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace api_manager {

namespace {

// If api_key is not provided, check if it is allowed.
Status CheckCallerIdentity(context::RequestContext *context) {
  if (!context->api_key().empty()) {
    // API Key was provided.
    return Status::OK;
  }

  const MethodInfo *method_info = context->method();
  if (method_info != nullptr && method_info->allow_unregistered_calls() &&
      !context->service_context()->project_id().empty()) {
    // Project ID is available.
    return Status::OK;
  }

  return Status(Code::UNAUTHENTICATED,
                "Method doesn't allow unregistered callers (callers without "
                "established identity). Please use API Key or other form of "
                "API consumer identity to call this API.",
                Status::SERVICE_CONTROL);
}

}  // namespace

void CheckServiceControl(std::shared_ptr<context::RequestContext> context,
                         std::function<void(Status status)> continuation) {
  std::shared_ptr<cloud_trace::CloudTraceSpan> trace_span(
      CreateSpan(context->cloud_trace(), "CheckServiceControl"));
  // If the method is not configured from the service config.
  // or if not need to check service control, skip it.
  if (!context->method()) {
    TRACE(trace_span) << "Method is not configured in the service config";
    continuation(Status(Code::NOT_FOUND, "Method does not exist.",
                        Status::SERVICE_CONTROL));
    return;
  } else if (!context->service_context()->service_control()) {
    TRACE(trace_span) << "Service control check is not needed";
    continuation(Status::OK);
    return;
  }

  Status status = CheckCallerIdentity(context.get());
  if (!status.ok()) {
    TRACE(trace_span) << "Failed at checking caller identity.";
    continuation(status);
    return;
  }

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
        continuation(status);
      });
}

}  // namespace api_manager
}  // namespace google
