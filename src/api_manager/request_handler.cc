// Copyright (C) Extensible Service Proxy Authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "src/api_manager/request_handler.h"

#include "google/devtools/cloudtrace/v1/trace.pb.h"
#include "google/protobuf/stubs/logging.h"
#include "src/api_manager/auth/service_account_token.h"
#include "src/api_manager/check_workflow.h"
#include "src/api_manager/cloud_trace/cloud_trace.h"
#include "src/api_manager/utils/marshalling.h"

using ::google::api_manager::utils::Status;
using google::devtools::cloudtrace::v1::Traces;

namespace google {
namespace api_manager {

namespace {
// The time window to send intermediate report for Grpc streaming (second).
// Default to 10s.
const int kIntermediateReportInterval = 10;
// The threshold of data size for grpc streaming intermediate report (bytes).
// Default to 50KB.
const int kIntermediateReportThreshold = 50000;
}

RequestHandler::RequestHandler(
    std::shared_ptr<CheckWorkflow> check_workflow,
    std::shared_ptr<context::ServiceContext> service_context,
    std::unique_ptr<Request> request_data)
    : context_(new context::RequestContext(service_context,
                                           std::move(request_data))),
      check_workflow_(check_workflow) {
  intermediate_report_interval_ = kIntermediateReportInterval;
  intermediate_report_threshold_ = kIntermediateReportThreshold;

  // Check server_config override.
  if (context_->service_context()->config()->server_config() &&
      context_->service_context()
          ->config()
          ->server_config()
          ->has_streaming_report_config()) {
    proto::StreamingReportConfig streaming_report_config =
        context_->service_context()
            ->config()
            ->server_config()
            ->streaming_report_config();
    if (streaming_report_config.intermediate_time_window()) {
      intermediate_report_interval_ =
          streaming_report_config.intermediate_time_window();
    }
    if (streaming_report_config.threshold_in_bytes()) {
      intermediate_report_threshold_ =
          streaming_report_config.threshold_in_bytes();
    }
  }
}

void RequestHandler::Check(std::function<void(Status status)> continuation) {
  auto interception = [continuation, this](Status status) {
    if (status.ok() && context_->cloud_trace()) {
      context_->StartBackendSpanAndSetTraceContext();
    }
    continuation(status);
  };

  context_->set_check_continuation(interception);

  // Run the check flow.
  check_workflow_->Run(context_);
}

void RequestHandler::AttemptIntermediateReport() {
  // For grpc streaming calls, we send intermediate reports to represent
  // streaming stats. Specifically:
  // 1) We send request_count in the first report to indicate the start of a
  // stream.
  // 2) We send request_bytes, response_bytes in intermediate reports, which
  // triggered by timer.
  // 3) In the final report, we send all metrics except request_count if it
  // already sent.
  service_control::ReportRequestInfo info;
  info.is_first_report = context_->is_first_report();
  info.is_final_report = false;
  context_->FillReportRequestInfo(NULL, &info);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  // We only send intermediate streaming report if the transferred_data_size >
  // intermediate_report_threshold_ and the time_interval >
  // intermediate_report_interval_.
  if ((info.request_bytes - context_->get_last_request_bytes() >=
           intermediate_report_threshold_ ||
       info.response_bytes - context_->get_last_response_bytes() >=
           intermediate_report_threshold_) &&
      std::chrono::duration_cast<std::chrono::seconds>(
          now - context_->get_last_report_time())
              .count() >= intermediate_report_interval_) {
    // Calling service_control Report.
    Status status =
        context_->service_context()->service_control()->Report(info);
    if (!status.ok()) {
      context_->service_context()->env()->LogError(
          "Failed to send intermediate report to service control.");
    } else {
      context_->set_first_report(false);
      context_->set_last_report_time(now);
      context_->set_last_request_bytes(info.request_bytes);
      context_->set_last_response_bytes(info.response_bytes);
    }
  }
}

// Sends a report.
void RequestHandler::Report(std::unique_ptr<Response> response,
                            std::function<void(void)> continuation) {
  // Close backend trace span.
  context_->EndBackendSpan();

  if (context_->service_context()->service_control()) {
    service_control::ReportRequestInfo info;
    info.is_first_report = context_->is_first_report();
    info.is_final_report = true;
    context_->FillReportRequestInfo(response.get(), &info);
    // Calling service_control Report.
    Status status =
        context_->service_context()->service_control()->Report(info);
    if (!status.ok()) {
      context_->service_context()->env()->LogError(
          "Failed to send report to service control.");
    }
  }

  if (context_->cloud_trace()) {
    context_->cloud_trace()->EndRootSpan();
    // Always set the project_id to the latest one.
    //
    // this is how project_id is calculated: if gce metadata is fetched, use
    // its project_id. Otherwise, use the project_id from service_config if it
    // is configured.
    // gce metadata fetching is started by the first request. While fetching is
    // in progress, subsequent requests will fail.  These failed requests may
    // have wrong project_id until gce metadata is fetched successfully.
    context_->service_context()->cloud_trace_aggregator()->SetProjectId(
        context_->service_context()->project_id());
    context_->service_context()->cloud_trace_aggregator()->AppendTrace(
        context_->cloud_trace()->ReleaseTrace());
  }

  continuation();
}

std::string RequestHandler::GetBackendAddress() const {
  if (context_->method()) {
    return context_->method()->backend_address();
  } else {
    return std::string();
  }
}

std::string RequestHandler::GetRpcMethodFullName() const {
  if (context_ && context_->method() &&
      !context_->method()->rpc_method_full_name().empty()) {
    return context_->method()->rpc_method_full_name();
  } else {
    return std::string();
  }
}

}  // namespace api_manager
}  // namespace google
