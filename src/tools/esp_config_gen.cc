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
#include <fstream>
#include <iostream>

#include "google/api/service.pb.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "src/api_manager/utils/marshalling.h"

const int kSourceFile = 1;
const int kDestinationFile = 2;
const int kJsonOutput = 3;
const int kTextOutput = 4;
const int kBinaryOutput = 5;
const int kServiceName = 6;
const int kServiceControl = 7;
const int kStripConsumer = 8;
const int kHelp = 20;

static struct option options[] = {
    {"src", required_argument, nullptr, kSourceFile},
    {"dst", required_argument, nullptr, kDestinationFile},
    {"json", no_argument, nullptr, kJsonOutput},
    {"text", no_argument, nullptr, kTextOutput},
    {"binary", no_argument, nullptr, kBinaryOutput},
    {"service_name", required_argument, nullptr, kServiceName},
    {"service_control", required_argument, nullptr, kServiceControl},
    {"strip_consumer", no_argument, nullptr, kStripConsumer},
    {"help", no_argument, nullptr, kHelp},
    {0, 0, 0, 0},
};

void usage(const char *program) {
  std::cerr
      << "Usage: " << program
      << " [options]\n"
         "Creates an API service config with only the values used by ESP.\n\n"
         "With the following command-line options supported:\n"
         "  --src <source file>\n"
         "    File with a serialized service config, text format or binary.\n"
         "  --dst <destination file>\n"
         "    Output file into which to save the processed service config.\n"
         "  --service_name <service name>\n"
         "    Set a specific service name in the output config.\n"
         "  --service_control <service control address>\n"
         "    An address of the service control to use with the API.\n"
         "    Default is servicecontrol.googleapis.com.\n"
         "  --strip_consumer\n"
         "    Remove consumer metrics/logs from the configuration so ESP \n"
         "    won't report it. (default is to keep consumer config)\n"
         "  --text\n"
         "    Save --dst file in a text format (default is binary)\n"
         "  --json\n"
         "    Save --dst file in a JSON format (default is binary)\n"
         "  --binary\n"
         "    Save --dst file in a binary proto format (default).\n";
}

bool ParseConfig(std::istream &src, ::google::api::Service *service) {
  ::google::protobuf::TextFormat::Parser parser;

  std::string contents;
  src.seekg(0, std::ios::end);
  contents.reserve(src.tellg());
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

enum OutputType { BINARY, TEXT, JSON };

bool Output(const ::google::api::Service &service, OutputType ot,
            std::ostream &dst) {
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

int main(int argc, char **argv) {
  const char *src_path = 0;
  const char *dst_path = 0;
  const char *service_name = 0;
  const char *service_control = 0;
  OutputType ot = BINARY;
  bool strip_consumer = false;

  for (;;) {
    int option = getopt_long(argc, argv, "", options, nullptr);
    if (option == -1) {
      break;
    }

    switch (option) {
      case kSourceFile:
        src_path = optarg;
        break;
      case kDestinationFile:
        dst_path = optarg;
        break;
      case kServiceName:
        service_name = optarg;
        break;
      case kServiceControl:
        service_control = optarg;
        break;
      case kTextOutput:
        ot = TEXT;
        break;
      case kJsonOutput:
        ot = JSON;
        break;
      case kBinaryOutput:
        ot = BINARY;
        break;
      case kStripConsumer:
        strip_consumer = true;
        break;
      default:
        usage(argv[0]);
        return 1;
    }
  }

  if (!src_path) {
    usage(argv[0]);
    return 1;
  }

  std::ifstream src;
  src.open(src_path, std::ifstream::in | std::ifstream::binary);

  ::google::api::Service service;
  if (!ParseConfig(src, &service)) {
    std::cerr << "ERROR: Cannot parse google.api.Service from " << src_path
              << "\n";
    return 1;
  }

  if (service_name) {
    service.set_name(service_name);
    std::cerr << "Setting service name to " << service.name() << "\n";
  }

  if (service_control) {
    service.mutable_control()->set_environment(service_control);
    std::cerr << "Setting service control to "
              << service.control().environment() << "\n";
  }

  if (strip_consumer) {
    service.mutable_monitoring()->clear_consumer_destinations();
    std::cerr << "Cleared consumer monitoring destinations.\n";
  }

  bool success;
  if (dst_path) {
    std::ofstream dst;
    dst.open(dst_path, std::ofstream::out | std::ofstream::binary);
    success = Output(service, ot, dst);
  } else {
    success = Output(service, ot, std::cout);
  }

  if (!success) {
    std::cerr << "ERROR: Cannot serialize google.api.Service to " << dst_path
              << "\n";
    return 1;
  }
  return 0;
}
