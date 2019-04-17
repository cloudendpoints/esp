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
#include <fstream>
#include <iostream>

#include "google/protobuf/text_format.h"
#include "src/api_manager/proto/server_config.pb.h"
#include "src/api_manager/utils/marshalling.h"

bool ParseConfig(std::istream &src, ::google::api_manager::proto::ServerConfig *service) {
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

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr  << "Usage: " << argv[0] << " file" << std::endl;
    return 1;
  }
  const char *src_path = argv[1];
  std::ifstream src;
  src.open(src_path, std::ifstream::in | std::ifstream::binary);

  ::google::api_manager::proto::ServerConfig config;
  if (!ParseConfig(src, &config)) {
    std::cerr << "ERROR: Cannot parse server_config from " << src_path << std::endl;
    return 1;
  }

  return 0;
}
