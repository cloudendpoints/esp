// Copyright (C) Endpoints Server Proxy Authors
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
#include <stdio.h>
#include <string.h>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "google/protobuf/text_format.h"
#include "src/api_manager/service_control/info.h"
#include "src/api_manager/service_control/proto.h"
#include "third_party/service-control-client-cxx/include/service_control_client.h"

using google::api_manager::service_control::OperationInfo;
using google::api_manager::service_control::ReportRequestInfo;
using google::api_manager::service_control::Proto;

using ::google::api::servicecontrol::v1::CheckRequest;
using ::google::api::servicecontrol::v1::CheckResponse;
using ::google::api::servicecontrol::v1::ReportRequest;
using ::google::api::servicecontrol::v1::ReportResponse;
using ::google::protobuf::Arena;
using ::google::protobuf::util::Status;

using ::google::service_control_client::CheckAggregationOptions;
using ::google::service_control_client::ReportAggregationOptions;
using ::google::service_control_client::ServiceControlClient;
using ::google::service_control_client::ServiceControlClientOptions;
using ::google::service_control_client::TransportDoneFunc;

namespace {

const char kServiceName[] = "library.googleapis.com";
const char kServiceConfigId[] = "2016-09-19r0";
const int MAX_PROTO_PASS_SIZE = 1000000;

}  //  namespace

void FillOperationInfo(OperationInfo* op) {
  op->operation_id = "operation_id";
  op->operation_name = "operation_name";
  op->api_key = "api_key_x";
  op->producer_project_id = "project_id";
}

void FillReportRequestInfo(ReportRequestInfo* request) {
  request->referer = "referer";
  request->response_code = 200;
  request->location = "us-central";
  request->api_name = "api-version";
  request->api_method = "api-method";
  request->request_size = 100;
  request->response_size = 1024 * 1024;
  request->log_message = "test-method is called";
  request->latency.request_time_ms = 123;
  request->latency.backend_time_ms = 101;
  request->latency.overhead_time_ms = 22;
  request->auth_issuer = "auth-issuer";
  request->auth_audience = "auth-audience";
}

int total_called_checks = 0;
int total_called_reports = 0;
std::string request_text;

// Compare the performance for passing Service Control Report protobuf to its
// aggregator.
// 1. Allocate a new protobuf for each call.
// 2. Re-use protobuf from a pool.
// 3. Use proto arena allocation.
int main() {
  Proto scp({"local_test_log"}, kServiceName, kServiceConfigId);

  std::unique_ptr<ServiceControlClient> client;
  // To set the cache, flush interval large enough for this test.
  ServiceControlClientOptions options(
      CheckAggregationOptions(1000000 /*entries*/,
                              1000000 /* refresh_interval_ms */,
                              1000000 /*flush_interval_ms*/),
      ReportAggregationOptions(1000000 /*entries*/,
                               1000000 /* refresh_interval_ms */));
  options.check_transport = [](const CheckRequest&, CheckResponse*,
                               TransportDoneFunc) { ++total_called_checks; };
  options.report_transport = [](const ReportRequest&, ReportResponse*,
                                TransportDoneFunc) { ++total_called_reports; };
  client = CreateServiceControlClient(kServiceName, kServiceConfigId, options);

  ReportRequestInfo info;
  FillOperationInfo(&info);
  FillReportRequestInfo(&info);
  ReportResponse response;

  // 1. Allocate proto for each call.
  std::clock_t start = std::clock();
  for (int i = 0; i < MAX_PROTO_PASS_SIZE; i++) {
    std::unique_ptr<ReportRequest> request(new ReportRequest());
    scp.FillReportRequest(info, request.get());
    client->Report(*request, &response, [](Status status) {});
  }

  GOOGLE_LOG(INFO) << "Report 1 million requests time: "
                   << 1000.0 * (std::clock() - start) / CLOCKS_PER_SEC << "ms";
  GOOGLE_CHECK(total_called_reports == 0);
  client = CreateServiceControlClient(kServiceName, kServiceConfigId, options);
  GOOGLE_CHECK(total_called_reports == 1);
  total_called_reports = 0;

  // 2. Reuse proto allocation.
  std::clock_t start_reuse = std::clock();
  std::unique_ptr<ReportRequest> request_reuse(new ReportRequest());
  for (int i = 0; i < MAX_PROTO_PASS_SIZE; i++) {
    scp.FillReportRequest(info, request_reuse.get());
    client->Report(*request_reuse, &response, [](Status status) {});
    request_reuse->Clear();
  }

  GOOGLE_LOG(INFO) << "Report 1 million requests reuse the proto: "
                   << 1000.0 * (std::clock() - start_reuse) / CLOCKS_PER_SEC
                   << "ms";
  GOOGLE_CHECK(total_called_reports == 0);
  client = CreateServiceControlClient(kServiceName, kServiceConfigId, options);
  // GOOGLE_CHECK(total_called_reports == 1) does not hold here.
  total_called_reports = 0;

  // 3. Use proto arena allocation.
  std::clock_t start_arena = std::clock();
  for (int i = 0; i < MAX_PROTO_PASS_SIZE; i++) {
    std::unique_ptr<Arena> arena(new Arena);
    ReportRequest* request_arena =
        Arena::CreateMessage<ReportRequest>(arena.get());
    scp.FillReportRequest(info, request_arena);
    client->Report(*request_arena, &response, [](Status status) {});
  }

  GOOGLE_LOG(INFO) << "Report 1 million requests using arena:"
                   << 1000.0 * (std::clock() - start_arena) / CLOCKS_PER_SEC
                   << "ms";
  GOOGLE_CHECK(total_called_reports == 0);
  client = CreateServiceControlClient(kServiceName, kServiceConfigId, options);
  GOOGLE_CHECK(total_called_reports == 1);

  return 0;
}
