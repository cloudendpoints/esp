// Copyright (C) Extensible Service Proxy Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "src/api_manager/fetch_metadata.h"

#include "src/api_manager/context/global_context.h"
#include "src/api_manager/context/service_context.h"
#include "src/api_manager/mock_api_manager_environment.h"
#include "src/api_manager/mock_request.h"

using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::_;

using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace api_manager {

namespace {

const char kServiceConfig[] = R"(
{
  "name": "endpoints-test.cloudendpointsapis.com",
  "control": {
     "environment": "http://127.0.0.1:808"
  }
})";

const char kServerConfig[] = R"(
{
  "metadata_server_config": {
     "enabled": true,
     "url": "http://127.0.0.1:8090"
   }
})";

const char kToken[] = R"(
{
  "access_token":"TOKEN",
  "expires_in":100,
  "token_type":"Bearer"
}
)";

const char kEmptyBody[] = R"({})";

class FetchMetadataTest : public ::testing::Test {
 public:
  void SetUp() {
    std::unique_ptr<MockApiManagerEnvironment> env(
        new ::testing::NiceMock<MockApiManagerEnvironment>());
    // save the raw pointer of env before calling std::move(env).
    raw_env_ = env.get();

    std::unique_ptr<Config> config = Config::Create(raw_env_, kServiceConfig);
    ASSERT_NE(config.get(), nullptr);

    global_context_ =
        std::make_shared<context::GlobalContext>(std::move(env), kServerConfig);
    service_context_ = std::make_shared<context::ServiceContext>(
        global_context_, std::move(config));

    ASSERT_NE(service_context_.get(), nullptr);

    std::unique_ptr<MockRequest> request(
        new ::testing::NiceMock<MockRequest>());

    context_ = std::make_shared<context::RequestContext>(service_context_,
                                                         std::move(request));
  }

  MockApiManagerEnvironment *raw_env_;
  std::shared_ptr<context::GlobalContext> global_context_;
  std::shared_ptr<context::ServiceContext> service_context_;
  std::shared_ptr<context::RequestContext> context_;
};

TEST_F(FetchMetadataTest, FetchTokenWithStatusOK) {
  // FetchGceMetadata responses with headers and status OK.
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_))
      .WillOnce(Invoke([](HTTPRequest *req) {
        std::map<std::string, std::string> empty;
        std::string body(kToken);
        req->OnComplete(Status::OK, std::move(empty), std::move(body));
      }));

  FetchServiceAccountToken(context_, [this](Status status) {
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(context_->service_context()
                  ->global_context()
                  ->service_account_token()
                  ->state(),
              auth::ServiceAccountToken::FETCHED);
  });
}

TEST_F(FetchMetadataTest, FetchTokenWithStatusINTERNAL) {
  // FetchGceMetadata responses with headers and status UNAVAILABLE.
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_))
      .WillOnce(Invoke([](HTTPRequest *req) {
        std::map<std::string, std::string> empty;
        std::string body(kEmptyBody);
        req->OnComplete(Status(Code::UNKNOWN, ""), std::move(empty),
                        std::move(body));
      }));

  FetchServiceAccountToken(context_, [this](Status status) {
    ASSERT_FALSE(status.ok());
    ASSERT_EQ(context_->service_context()
                  ->global_context()
                  ->service_account_token()
                  ->state(),
              auth::ServiceAccountToken::FAILED);
  });
}

}  // namespace

}  // namespace api_manager
}  // namespace google
