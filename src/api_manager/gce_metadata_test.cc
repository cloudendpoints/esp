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
#include "src/api_manager/gce_metadata.h"
#include "gtest/gtest.h"

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {

namespace {

const char metadata[] =
    "{\n"
    "  \"instance\": {\n"
    "    \"attributes\": {\n"
    "      \"gae_app_container\": \"app\", \n"
    "      \"gae_app_fullname\": "
    "\"test-app_20150921t180445-387321214075436208\", \n"
    "      \"gae_backend_instance\": \"0\", \n"
    "      \"gae_backend_minor_version\": \"387321214075436208\", \n"
    "      \"gae_backend_name\": \"default\", \n"
    "      \"gae_backend_version\": \"20150921t180445\", \n"
    "      \"gae_partition\": \"s\", \n"
    "      \"gae_project\": \"test-app\", \n"
    "      \"gcm-pool\": \"gae-default-20150921t180445\", \n"
    "      \"gcm-replica\": \"gae-default-20150921t180445-inqp\" \n"
    "    }, \n"
    "    \"hostname\": "
    "\"gae-default-20150921t180445-inqp.c.test-app.internal\", \n"
    "    \"id\": 3296474103533342935, \n"
    "    \"zone\": \"projects/23479234856/zones/us-central1-f\"\n"
    "  }, \n"
    "  \"project\": {\n"
    "    \"attributes\": {\n"
    "      \"google-compute-default-region\": \"us-central1\", \n"
    "      \"google-compute-default-zone\": \"us-central1-f\" \n"
    "    }, \n"
    "    \"numericProjectId\": 23479234856, \n"
    "    \"projectId\": \"test-app\"\n"
    "  }\n"
    "}";

const char partial_metadata[] =
    "{\n"
    "  \"instance\": {\n"
    "    \"hostname\": "
    "\"gae-default-20150921t180445-inqp.c.test-app.internal\", \n"
    "    \"zone\": \"projects/23479234856/zones/us-central1-f\"\n"
    "  }\n"
    "}";

TEST(Metadata, ExtractPropertyValue) {
  GceMetadata env;
  std::string meta_str(metadata);
  Status status = env.ParseFromJson(&meta_str);
  ASSERT_TRUE(status.ok());

  ASSERT_EQ("gae-default-20150921t180445-inqp.c.test-app.internal",
            env.hostname());
  ASSERT_EQ("us-central1-f", env.zone());
  ASSERT_EQ("default", env.gae_backend_name());
  ASSERT_EQ("20150921t180445", env.gae_backend_version());
  ASSERT_EQ("test-app", env.project_id());
}

TEST(Metadata, ExtractSomePropertyValues) {
  GceMetadata env;
  std::string meta_str(partial_metadata);
  Status status = env.ParseFromJson(&meta_str);
  ASSERT_TRUE(status.ok());
  ASSERT_EQ("gae-default-20150921t180445-inqp.c.test-app.internal",
            env.hostname());
  ASSERT_EQ("us-central1-f", env.zone());
}

TEST(Metadata, ExtractErrors) {
  std::string meta_str(metadata);
  std::string half_str = meta_str.substr(0, meta_str.size() / 2);
  // Half a Json to make it an invalid Json.
  GceMetadata env;
  Status status = env.ParseFromJson(&half_str);
  ASSERT_FALSE(status.ok());
}

}  // namespace

}  // namespace api_manager
}  // namespace google
