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
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "gflags/gflags.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/type_resolver.h"
#include "google/protobuf/util/type_resolver_util.h"
#include "grpc++/grpc++.h"
#include "test/transcoding/bookstore.grpc.pb.h"
#include "test/transcoding/bookstore.pb.h"

using endpoints::examples::bookstore::Bookstore;
using endpoints::examples::bookstore::CreateShelfRequest;
using endpoints::examples::bookstore::ListShelvesResponse;
using endpoints::examples::bookstore::Shelf;

template <class MessageType>
void Check(bool b, int error_code, const MessageType& error_message) {
  if (!b) {
    std::cerr << error_message << std::endl;
    exit(error_code);
  }
}

template <class StatusType>
void CheckStatus(const StatusType& status) {
  Check(status.ok(), status.error_code(), status.error_message());
}

void PrintResult(const ::google::protobuf::Message& m) {
  static ::google::protobuf::util::TypeResolver* type_resolver =
      ::google::protobuf::util::NewTypeResolverForDescriptorPool(
          "type.googleapis.com", m.GetDescriptor()->file()->pool());

  std::string binary;
  Check(m.SerializeToString(&binary), 1,
        "ERROR: could not serialize the response message");

  std::string json;
  CheckStatus(::google::protobuf::util::BinaryToJsonString(
      type_resolver, "type.googleapis.com/" + m.GetDescriptor()->full_name(),
      binary, &json));

  std::cout << json;
  std::cout.flush();
}

DEFINE_string(backend, "localhost:8081", "gRPC backend address");
DEFINE_bool(use_ssl, false, "whether to use SSL or not");
DEFINE_string(method, "ListShelves", "method to call");
DEFINE_string(parameter, "", "parameter to supply to the method");
DEFINE_string(api_key, "", "API Key");

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // Create a Bookstore stub using the specified server address.
  auto stub = Bookstore::NewStub(::grpc::CreateChannel(
      FLAGS_backend,
      FLAGS_use_ssl ? ::grpc::SslCredentials(::grpc::SslCredentialsOptions{})
                    : ::grpc::InsecureChannelCredentials()));

  ::grpc::ClientContext ctx;
  if (!FLAGS_api_key.empty()) {
    ctx.AddMetadata("x-api-key", FLAGS_api_key);
  }

  if (FLAGS_method == "ListShelves") {
    // ListShelves
    ::google::protobuf::Empty e;
    ListShelvesResponse shelves;
    CheckStatus(stub->ListShelves(&ctx, e, &shelves));
    PrintResult(shelves);
  } else if (FLAGS_method == "CreateShelf") {
    // CreateShelf
    CreateShelfRequest r;
    r.mutable_shelf()->set_theme(FLAGS_parameter.empty() ? "History"
                                                         : FLAGS_parameter);
    Shelf shelf;
    CheckStatus(stub->CreateShelf(&ctx, r, &shelf));
    PrintResult(shelf);
  } else if (FLAGS_method == "BulkCreateShelves") {
    // BulkCreateShelves
    auto stream = stub->BulkCreateShelf(&ctx);

    // Write all themes to the stream
    std::istringstream ss(FLAGS_parameter);
    while (!ss.eof()) {
      std::string theme;
      ss >> theme;
      CreateShelfRequest r;
      r.mutable_shelf()->set_theme(theme);
      stream->Write(r);
    }
    stream->WritesDone();

    // Read the responses from the stream
    std::cout << "[";
    bool first = true;
    Shelf shelf;
    while (stream->Read(&shelf)) {
      if (!first) {
        std::cout << ", ";
      }
      first = false;
      PrintResult(shelf);
    }
    std::cout << "]";

    CheckStatus(stream->Finish());
  } else {
    std::cerr << "Invalid method" << std::endl;
    return 1;
  }

  return 0;
}
