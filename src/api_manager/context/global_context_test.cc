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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "src/api_manager/context/global_context.h"
#include "src/api_manager/mock_api_manager_environment.h"

namespace google {
namespace api_manager {
namespace context {

TEST(GlobalContextTest, TestEmptyExperimentalFlags) {
  const char kServerConfig[] = "";

  std::unique_ptr<ApiManagerEnvInterface> env(
      new testing::NiceMock<MockApiManagerEnvironment>());
  GlobalContext ctx(std::move(env), kServerConfig);

  EXPECT_FALSE(ctx.DisableLogStatus());
  EXPECT_FALSE(ctx.AlwaysPrintPrimitiveFields());
  EXPECT_EQ(ctx.rollout_strategy(), "");

  EXPECT_EQ(ctx.platform(), compute_platform::UNKNOWN);
  EXPECT_EQ(ctx.project_id(), "");
  EXPECT_EQ(ctx.location(), "");

  EXPECT_EQ(ctx.service_account_token()->state(),
            auth::ServiceAccountToken::NONE);
}

TEST(GlobalContextTest, TestExperimentalFlags) {
  const char kServerConfig[] = R"(
experimental {
  disable_log_status: true
  always_print_primitive_fields: true
}
rollout_strategy: "fixed"
  )";

  std::unique_ptr<ApiManagerEnvInterface> env(
      new testing::NiceMock<MockApiManagerEnvironment>());
  GlobalContext ctx(std::move(env), kServerConfig);
  EXPECT_TRUE(ctx.DisableLogStatus());
  EXPECT_TRUE(ctx.AlwaysPrintPrimitiveFields());
  EXPECT_EQ(ctx.rollout_strategy(), "fixed");
}

TEST(GlobalContextTest, TestMetadataAttributes) {
  const char kServerConfig[] = R"(
metadata_attributes {
  project_id: "PROJECT_ID"
  zone: "us-west1-a"
  gae_server_software: "abd"
  access_token: {
    access_token: "TOKEN"
    expires_in: 3150
    token_type: "Bearer"
  }
}
)";

  std::unique_ptr<ApiManagerEnvInterface> env(
      new testing::NiceMock<MockApiManagerEnvironment>());
  GlobalContext ctx(std::move(env), kServerConfig);

  EXPECT_EQ(ctx.platform(), compute_platform::GAE_FLEX);
  EXPECT_EQ(ctx.project_id(), "PROJECT_ID");
  EXPECT_EQ(ctx.location(), "us-west1-a");

  EXPECT_EQ(ctx.service_account_token()->state(),
            auth::ServiceAccountToken::FETCHED);
  EXPECT_TRUE(ctx.service_account_token()->is_access_token_valid(0));
  EXPECT_EQ(ctx.service_account_token()->GetAuthToken(
                auth::ServiceAccountToken::JWT_TOKEN_FOR_SERVICE_CONTROL),
            "TOKEN");
}

TEST(GlobalContextTest, TestInstanceIdentityTokenCache) {
  const char kServerConfig[] = R"(
metadata_attributes {
  project_id: "PROJECT_ID"
  zone: "us-west1-a"
)";

  std::unique_ptr<ApiManagerEnvInterface> env(
      new testing::NiceMock<MockApiManagerEnvironment>());

  GlobalContext ctx(std::move(env), kServerConfig);

  auto got_token = ctx.GetInstanceIdentityToken("test-audience");

  got_token->set_access_token("test_jwt_token", 200);

  EXPECT_EQ(got_token->GetAuthToken(), "test_jwt_token");

  EXPECT_EQ(got_token->is_access_token_valid(100), true);

  EXPECT_EQ(got_token->is_access_token_valid(210), false);

  auto non_exist_token =
      ctx.GetInstanceIdentityToken("non-exist-test-audience");

  EXPECT_EQ(non_exist_token->GetAuthToken(), "");
}

}  // namespace context
}  // namespace api_manager
}  // namespace google
