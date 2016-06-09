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
#include <string>

#include "google/protobuf/text_format.h"
#include "grpc++/grpc++.h"
#include "test/transcoding/bookstore.grpc.pb.h"
#include "test/transcoding/bookstore.pb.h"

using endpoints::examples::bookstore::Bookstore;
using endpoints::examples::bookstore::CreateShelfRequest;
using endpoints::examples::bookstore::Empty;
using endpoints::examples::bookstore::ListShelvesResponse;
using endpoints::examples::bookstore::Shelf;

template <typename MessageType>
void PrintResult(::grpc::Status status, const MessageType& m) {
  if (!status.ok()) {
    std::cerr << "Error " << status.error_code() << " - "
              << status.error_message() << std::endl;
  } else {
    std::string response;
    google::protobuf::TextFormat::PrintToString(m, &response);
    std::cout << response << std::endl;
  }
}

int main(int argc, char** argv) {
  // Create a Bookstore stub using the specified server address.
  auto stub = Bookstore::NewStub(
      ::grpc::CreateChannel(argc > 1 ? argv[1] : "localhost:8081",
                            grpc::InsecureChannelCredentials()));

  // Do some test calls

  // ListShelves
  {
    ::grpc::ClientContext ctx;
    Empty e;
    ListShelvesResponse shelves;
    ::grpc::Status status = stub->ListShelves(&ctx, e, &shelves);
    PrintResult(status, shelves);
  }

  // CreateShelf
  {
    ::grpc::ClientContext ctx;
    CreateShelfRequest r;
    r.mutable_shelf()->set_theme("History");
    Shelf shelf;
    ::grpc::Status status = stub->CreateShelf(&ctx, r, &shelf);
    PrintResult(status, shelf);
  }

  // BulkCreateShelves
  {
    ::grpc::ClientContext ctx;
    auto stream = stub->BulkCreateShelf(&ctx);

    for (int i = 3; i < argc; ++i) {
      CreateShelfRequest r;
      r.mutable_shelf()->set_theme(argv[i]);
      stream->Write(r);
    }
    stream->WritesDone();

    Shelf shelf;
    while (stream->Read(&shelf)) {
      PrintResult(::grpc::Status::OK, shelf);
    }
    ::grpc::Status status = stream->Finish();
    if (!status.ok()) {
      PrintResult(status, shelf);
    }
  }

  return 0;
}
