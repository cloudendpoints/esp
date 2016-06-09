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
#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/type_resolver.h"
#include "google/protobuf/util/type_resolver_util.h"
#include "grpc++/grpc++.h"
#include "test/transcoding/bookstore.grpc.pb.h"
#include "test/transcoding/bookstore.pb.h"

namespace endpoints {
namespace examples {
namespace bookstore {

// Prints the request message as JSON to STDOUT
template <typename MessageType>
void PrintRequest(const MessageType& message) {
  static ::google::protobuf::util::TypeResolver* type_resolver =
      ::google::protobuf::util::NewTypeResolverForDescriptorPool(
          "type.googleapis.com", message.GetDescriptor()->file()->pool());

  std::string binary;
  if (!message.SerializeToString(&binary)) {
    std::cout << "ERROR: could not serialize the request message" << std::endl;
    return;
  }

  std::string json;
  auto status = ::google::protobuf::util::BinaryToJsonString(
      type_resolver,
      "type.googleapis.com/" + message.GetDescriptor()->full_name(), binary,
      &json);
  if (!status.ok()) {
    std::cout << "ERROR: " << status.error_message() << std::endl;
    return;
  }

  static const char delimiter[] = "\r\n\r\n";
  std::cout << json << delimiter;
  std::cout.flush();
}

// Helper to generate names for books & shelves
template <typename T>
int IdGenerator() {
  static int i = 1;
  return i++;
}

// A memory based Bookstore Service
class BookstoreServiceImpl : public Bookstore::Service {
 public:
  BookstoreServiceImpl() {
    // Create sample shelves
    shelves_.emplace_back(CreateShelfObject("Fiction"));
    shelves_.emplace_back(CreateShelfObject("Fantasy"));

    // Create sample books
    books_.insert(std::make_pair(
        shelves_[0].id(), CreateBookObject("Neal Stephenson", "Readme")));

    books_.insert(std::make_pair(
        shelves_[1].id(),
        CreateBookObject("George R.R. Martin", "A Game of Thrones")));
  }

  // Bookstore::Service implementation

  ::grpc::Status ListShelves(::grpc::ServerContext*, const Empty* request,
                             ListShelvesResponse* reply) {
    std::cerr << "GRPC-BACKEND: ListShelves" << std::endl;
    PrintRequest(*request);

    for (const auto& shelf : shelves_) {
      *reply->add_shelves() = shelf;
    }
    return ::grpc::Status::OK;
  }

  ::grpc::Status CreateShelf(::grpc::ServerContext*,
                             const CreateShelfRequest* request, Shelf* reply) {
    std::cerr << "GRPC-BACKEND: CreateShelf" << std::endl;
    PrintRequest(*request);

    auto shelf = request->shelf();
    if (0 == shelf.id()) {
      shelf.set_id(IdGenerator<Shelf>());
    }
    *reply = shelf;
    shelves_.emplace_back(std::move(shelf));
    return ::grpc::Status::OK;
  }

  ::grpc::Status BulkCreateShelf(
      ::grpc::ServerContext*,
      grpc::ServerReaderWriter<Shelf, CreateShelfRequest>* stream) override {
    std::cerr << "GRPC-BACKEND: BulkCreateShelf" << std::endl;
    // No need to print the request as CreateShelf() call below will do

    CreateShelfRequest request;
    while (stream->Read(&request)) {
      Shelf reply;
      auto status = CreateShelf(nullptr, &request, &reply);
      if (!status.ok()) {
        return status;
      }

      stream->Write(reply);
    }

    return ::grpc::Status::OK;
  }

  ::grpc::Status GetShelf(::grpc::ServerContext*,
                          const GetShelfRequest* request, Shelf* reply) {
    std::cerr << "GRPC-BACKEND: GetShelf" << std::endl;
    PrintRequest(*request);

    int id = request->shelf();
    for (const auto& shelf : shelves_) {
      if (id == shelf.id()) {
        *reply = shelf;
        return ::grpc::Status::OK;
      }
    }

    return ::grpc::Status(grpc::NOT_FOUND, "Shelf not found");
  }

  ::grpc::Status DeleteShelf(::grpc::ServerContext*,
                             const DeleteShelfRequest* request, Empty* reply) {
    std::cerr << "GRPC-BACKEND: DeleteShelf" << std::endl;
    PrintRequest(*request);

    auto id = request->shelf();
    auto it =
        std::find_if(std::begin(shelves_), std::end(shelves_),
                     [id](const Shelf& shelf) { return id == shelf.id(); });

    if (it != std::end(shelves_)) {
      shelves_.erase(it);
      return ::grpc::Status::OK;
    } else {
      return ::grpc::Status(grpc::NOT_FOUND, "Shelf not found");
    }
  }

  ::grpc::Status ListBooks(::grpc::ServerContext*,
                           const ListBooksRequest* request,
                           ListBooksResponse* reply) {
    std::cerr << "GRPC-BACKEND: ListBooks" << std::endl;
    PrintRequest(*request);
    // Not implemented yet
    return ::grpc::Status::OK;
  }

  ::grpc::Status CreateBook(::grpc::ServerContext*,
                            const CreateBookRequest* request, Book* reply) {
    std::cerr << "GRPC-BACKEND: CreateBook" << std::endl;
    PrintRequest(*request);
    // Not implemented yet
    return ::grpc::Status::OK;
  }

  ::grpc::Status GetBook(::grpc::ServerContext*, const GetBookRequest* request,
                         Book* reply) {
    std::cerr << "GRPC-BACKEND: GetBook" << std::endl;
    PrintRequest(*request);
    // Not implemented yet
    return ::grpc::Status::OK;
  }

  ::grpc::Status DeleteBook(::grpc::ServerContext*,
                            const DeleteBookRequest* request, Empty* reply) {
    std::cerr << "GRPC-BACKEND: DeleteBook" << std::endl;
    PrintRequest(*request);
    // Not implemented yet
    return ::grpc::Status::OK;
  }

 private:
  // A helper to create shelves
  static Shelf CreateShelfObject(std::string theme) {
    Shelf shelf;
    shelf.set_id(IdGenerator<Shelf>());
    shelf.set_theme(theme);
    return shelf;
  }

  // A helper to create books
  static Book CreateBookObject(std::string author, std::string title) {
    Book book;
    book.set_author(author);
    book.set_id(IdGenerator<Book>());
    book.set_title(title);
    return book;
  }

  // The database of shelves and books
  std::vector<Shelf> shelves_;
  std::map<int, Book> books_;
};

}  // namespace bookstore {
}  // namespace examples {
}  // namespace endpoints {

int main(int argc, char** argv) {
  auto address = argc > 1 ? argv[1] : "localhost:8081";

  // Bring up the server at the specified address.
  ::endpoints::examples::bookstore::BookstoreServiceImpl service;
  ::grpc::ServerBuilder builder;
  builder.AddListeningPort(address, ::grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  auto server = builder.BuildAndStart();
  std::cerr << "Server listening on " << address << std::endl;
  server->Wait();

  return 0;
}
