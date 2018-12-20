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
#include <getopt.h>
#include <stdlib.h>
#include <uuid/uuid.h>
#include <chrono>
#include <fstream>
#include <iostream>

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "src/api_manager/service_control/proto.h"
#include "src/api_manager/utils/marshalling.h"

const char* const DEFAULT_SERVICE_NAME =
    "endpoints-test.cloudendpointsapis.com";
const char* const DEFAULT_OPERATION_NAME = "test-operation";
const char* const DEFAULT_PRODUCER_PROJECT_ID = "endpoints-test";

enum OutputType { BINARY, TEXT, JSON };

enum ProtoMessageType {
  CHECK_REQUEST,
  CHECK_RESPONSE,
  QUOTA_REQUEST,
  QUOTA_RESPONSE,
  REPORT_REQUEST,
  REPORT_RESPONSE
};

/* Flag set by ‘--verbose’. */
static int verbose_flag;
static bool from_stdin = false;
const char* service_name = DEFAULT_SERVICE_NAME;
const char* operation_name = DEFAULT_OPERATION_NAME;
const char* producer_project_id = DEFAULT_PRODUCER_PROJECT_ID;
const char* src_path = 0;
const char* dst_path = 0;
OutputType output_format = JSON;
ProtoMessageType proto_type = CHECK_REQUEST;
size_t report_request_size = 0;

const int kSourceFile = 1;
const int kDestinationFile = 2;
const int kJsonOutput = 3;
const int kTextOutput = 4;
const int kBinaryOutput = 5;
const int kCheckRequstProto = 6;
const int kCheckResponseProto = 7;
const int kAllocateQuotaRequstProto = 8;
const int kAllocateQuotaResponseProto = 9;
const int kReportRequstProto = 10;
const int kReportResponseProto = 11;
const int kReportRequestSize = 12;

::google::api::servicecontrol::v1::CheckRequest check_request;
::google::api::servicecontrol::v1::CheckResponse check_response;
::google::api::servicecontrol::v1::AllocateQuotaRequest quota_request;
::google::api::servicecontrol::v1::AllocateQuotaResponse quota_response;
::google::api::servicecontrol::v1::ReportRequest report_request;
::google::api::servicecontrol::v1::ReportResponse report_response;

void ProcessCmdLine(int argc, char** argv) {
  int c;
  while (1) {
    static struct option long_options[] = {
        /* These options set a flag. */
        {"verbose", no_argument, &verbose_flag, 1},
        {"brief", no_argument, &verbose_flag, 0},
        /* These options don’t set a flag.
           We distinguish them by their indices. */
        {"src", required_argument, nullptr, kSourceFile},
        {"dst", required_argument, nullptr, kDestinationFile},
        {"json", no_argument, nullptr, kJsonOutput},
        {"text", no_argument, nullptr, kTextOutput},
        {"binary", no_argument, nullptr, kBinaryOutput},
        {"check_request", no_argument, nullptr, kCheckRequstProto},
        {"check_response", no_argument, nullptr, kCheckResponseProto},
        {"quota_request", no_argument, nullptr, kAllocateQuotaRequstProto},
        {"quota_response", no_argument, nullptr, kAllocateQuotaResponseProto},
        {"report_request", no_argument, nullptr, kReportRequstProto},
        {"report_response", no_argument, nullptr, kReportResponseProto},
        {"report_request_size", required_argument, nullptr, kReportRequestSize},
        {"stdin", no_argument, 0, 'i'},
        {"service_name", required_argument, 0, 's'},
        {"operation_name", required_argument, 0, 'o'},
        {"project_id", required_argument, 0, 'p'},
        {0, 0, 0, 0}};
    /* getopt_long stores the option index here. */
    int option_index = 0;
    c = getopt_long(argc, argv, "is:o:p:", long_options, &option_index);
    /* Detect the end of the options. */
    if (c == -1) {
      break;
    }
    switch (c) {
      case 0:
        break;
      case kSourceFile:
        src_path = optarg;
        break;
      case kDestinationFile:
        dst_path = optarg;
        break;
      case kTextOutput:
        output_format = TEXT;
        break;
      case kJsonOutput:
        output_format = JSON;
        break;
      case kBinaryOutput:
        output_format = BINARY;
        break;
      case kCheckRequstProto:
        proto_type = CHECK_REQUEST;
        break;
      case kCheckResponseProto:
        proto_type = CHECK_RESPONSE;
        break;
      case kAllocateQuotaRequstProto:
        proto_type = QUOTA_REQUEST;
        break;
      case kAllocateQuotaResponseProto:
        proto_type = QUOTA_RESPONSE;
        break;
      case kReportRequstProto:
        proto_type = REPORT_REQUEST;
        break;
      case kReportResponseProto:
        proto_type = REPORT_RESPONSE;
        break;
      case kReportRequestSize:
        proto_type = REPORT_REQUEST;
        report_request_size = strtol(optarg, nullptr, 10);
        break;
      case 'i':
        from_stdin = true;
        break;
      case 's':
        service_name = optarg;
        break;
      case 'o':
        operation_name = optarg;
        break;
      case 'p':
        producer_project_id = optarg;
        break;
      default:
        fprintf(
            stderr,
            "Usage:  service_control_json_gen options\n\n"
            "Generate or convert service control protobuf message.\n"
            "Protobuf message type:\n"
            "  --check_request:  CheckRequest protobuf message.\n"
            "  --check_response:  CheckResponse protobuf message.\n"
            "  --quota_request:  AllocateQuotaRequest protobuf message.\n"
            "  --quota_response:  AllocateQuotaResponse protobuf message.\n"
            "  --report_request:  ReportRequest protobuf message.\n"
            "  --report_response:  ReportResponse protobuf message.\n"
            "Input:\n"
            "  If -i, --stdin is specified, proto data is reading from stdin.\n"
            "  If --src is specified, proto data is reading from source file.\n"
            "  Otherwise, the proto data is fresh generated.\n"
            "Output:\n"
            "  If --dst is specified, write proto data to destination file.\n"
            "  Otherwise, write proto data to stdout.\n"
            "Output format:\n"
            "  --text: text format.\n"
            "  --json: json format.\n"
            "  --binary: binary format.\n"
            "Specify fields for freshly generated proto:\n"
            "  -s, --service_name name:  specify service name.\n"
            "  -o, --operation_name name:  specify operation name.\n"
            "  -p, --producer_project_id id:  specify producer project id.\n"
            "  --report_request_size size:  operations in ReportRequest will\n"
            "    be duplicated to reach the size.\n");
        exit(1);
    }
  }
}

bool ParseProto(std::istream& src, ::google::protobuf::Message* service) {
  std::string contents;
  src.seekg(0, std::ios::end);
  auto size = src.tellg();
  if (size > 0) {
    contents.reserve(size);
  }
  src.seekg(0, std::ios::beg);
  contents.assign((std::istreambuf_iterator<char>(src)),
                  (std::istreambuf_iterator<char>()));

  // Try JSON.
  ::google::api_manager::utils::Status status =
      ::google::api_manager::utils::JsonToProto(contents, service);
  if (status.ok()) {
    return true;
  }

  // Try binary.
  service->Clear();
  if (service->ParseFromString(contents)) {
    return true;
  }

  // Try text format.
  service->Clear();
  if (::google::protobuf::TextFormat::ParseFromString(contents, service)) {
    return true;
  }

  return false;
}

bool Output(const ::google::protobuf::Message& service, OutputType ot,
            std::ostream& dst) {
  switch (ot) {
    case TEXT: {
      ::google::protobuf::io::OstreamOutputStream output(&dst);
      return ::google::protobuf::TextFormat::Print(service, &output);
    }

    default:
    case JSON: {
      ::google::protobuf::io::OstreamOutputStream output(&dst);
      ::google::api_manager::utils::Status status =
          ::google::api_manager::utils::ProtoToJson(
              service, &output, ::google::api_manager::utils::PRETTY_PRINT);
      return status.ok();
    }

    case BINARY: {
      return service.SerializeToOstream(&dst);
    }
  }
}

std::string UuidGen() {
  char buf[40];  // Maximum 36 byte string.
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse(uuid, buf);
  return std::string(buf);
}

::google::protobuf::Message* generate_proto() {
  const std::set<std::string> logs = {"local_test_log"};

  std::string uuid = UuidGen();
  ::google::protobuf::Message* request = nullptr;
  if (proto_type == CHECK_REQUEST) {
    ::google::api_manager::service_control::CheckRequestInfo info;
    info.operation_id = uuid.c_str();
    info.operation_name = operation_name;
    info.producer_project_id = producer_project_id;
    info.client_ip = "1.2.3.4";
    info.referer = "referer";
    info.request_start_time = std::chrono::system_clock::now();

    ::google::api_manager::service_control::Proto scp(logs, service_name,
                                                      "2016-09-19r0");
    scp.FillCheckRequest(info, &check_request);
    request = &check_request;
  }
  if (proto_type == REPORT_REQUEST) {
    ::google::api_manager::service_control::ReportRequestInfo info;
    info.operation_id = uuid.c_str();
    info.operation_name = operation_name;
    info.producer_project_id = producer_project_id;

    info.referer = "referer";
    info.request_start_time = std::chrono::system_clock::now();
    ;
    info.response_code = 200;
    info.location = "us-central";
    info.api_name = "api-version";
    info.api_method = "api-method";
    info.request_size = 100;
    info.response_size = 1024 * 1024;
    info.log_message = "test-method is called";

    ::google::api_manager::service_control::Proto scp(logs, service_name,
                                                      "2016-09-19r0");
    scp.FillReportRequest(info, &report_request);
    request = &report_request;

    do {
      std::string out;
      report_request.SerializeToString(&out);
      if (out.size() >= report_request_size) {
        break;
      }
      // Duplicates operations to read this size
      *report_request.add_operations() = report_request.operations(0);
    } while (true);
  }
  return request;
}

int main(int argc, char** argv) {
  ProcessCmdLine(argc, argv);

  ::google::protobuf::Message* request = nullptr;
  switch (proto_type) {
    case CHECK_REQUEST:
      request = &check_request;
      break;
    case CHECK_RESPONSE:
      request = &check_response;
      break;
    case QUOTA_REQUEST:
      request = &quota_request;
      break;
    case QUOTA_RESPONSE:
      request = &quota_response;
      break;
    case REPORT_REQUEST:
      request = &report_request;
      break;
    case REPORT_RESPONSE:
      request = &report_response;
      break;
  }
  if (src_path) {
    std::ifstream src;
    src.open(src_path, std::ifstream::in | std::ifstream::binary);
    if (!ParseProto(src, request)) {
      std::cerr << "Failed to read proto from file: " << src_path << std::endl;
      return 1;
    }
  } else if (from_stdin) {
    if (!ParseProto(std::cin, request)) {
      std::cerr << "Failed to read proto from stdin." << std::endl;
      return 1;
    }
  } else {
    request = generate_proto();
  }

  if (request != nullptr) {
    bool success;
    if (dst_path) {
      std::ofstream dst;
      dst.open(dst_path, std::ofstream::out | std::ofstream::binary);
      success = Output(*request, output_format, dst);
    } else {
      success = Output(*request, output_format, std::cout);
    }
    if (!success) {
      std::cerr << "ERROR: Cannot serialize protobuf.\n";
      return 1;
    }
  }
  return 0;
}
