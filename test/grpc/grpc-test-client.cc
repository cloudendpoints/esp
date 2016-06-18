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
#include <iostream>

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "test/grpc/client-test-lib.h"
#include "test/grpc/grpc-test.grpc.pb.h"

using ::google::protobuf::io::IstreamInputStream;
using ::google::protobuf::io::OstreamOutputStream;
using ::google::protobuf::TextFormat;

int main(int argc, char **argv) {
  if (argc != 1) {
    std::cerr << "Usage: grpc-test-client" << std::endl;
    std::cerr << "Supply a text TestPlans proto on stdin to describe the tests."
              << std::endl;
    return EXIT_FAILURE;
  }

  ::test::grpc::TestPlans plans;
  ::test::grpc::TestResults results;
  std::cerr << "Parsing stdin" << std::endl;
  {
    IstreamInputStream in(&std::cin);
    if (!TextFormat::Parse(&in, &plans)) {
      std::cerr << "Failed to parse text TestPlans proto from stdin"
                << std::endl;
      return EXIT_FAILURE;
    }
  }
  std::cerr << "Running tests" << std::endl;

  ::test::grpc::RunTestPlans(plans, &results);

  std::cerr << "Writing test outputs" << std::endl;

  {
    OstreamOutputStream out(&std::cout);
    TextFormat::Print(results, &out);
  }

  std::cerr << "Exiting" << std::endl;
  return EXIT_SUCCESS;
}
