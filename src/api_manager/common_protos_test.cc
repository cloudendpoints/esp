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
#include "gtest/gtest.h"

#include "google/api/service.pb.h"
#include "google/api/servicecontrol/v1/service_controller.pb.h"
#include "src/api_manager/proto/json_error.pb.h"

// Trivial tests that instantiate common protos
// to make sure the compile and link correctly.

TEST(CommonProtos, ServiceConfig) {
  ::google::api::Service service;

  service.set_name("bookstore");

  ASSERT_EQ("bookstore", service.name());
}

TEST(CommonProtos, ServiceControl) {
  ::google::api::servicecontrol::v1::CheckRequest cr;

  cr.set_service_name("bookstore");
  cr.mutable_operation()->set_operation_name("CreateShelf");

  ASSERT_EQ("bookstore", cr.service_name());
  ASSERT_EQ("CreateShelf", cr.operation().operation_name());
}

TEST(CommonProtos, JsonError) {
  ::google::api_manager::proto::ErrorBody body;
  auto error = body.mutable_error();

  error->set_code(3);
  error->set_status(404);
  error->set_message("Error message");

  ASSERT_EQ(3, body.error().code());
  ASSERT_EQ(404, body.error().status());
  ASSERT_EQ("Error message", body.error().message());
}
