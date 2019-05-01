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
#include "src/api_manager/auth/lib/json.h"
#include "gtest/gtest.h"
#include "src/api_manager/auth/lib/auth_token.h"

namespace google {
namespace api_manager {
namespace auth {

void VerifyWriteUserInfo(const char* expected_json, const UserInfo& user_info) {
  char* json_str = WriteUserInfoToJson(user_info);
  ASSERT_STREQ(expected_json, json_str);
  esp_grpc_free(json_str);
}

TEST(EspJsonTest, NormalDataTest) {
  UserInfo user_info{"id", "email", "consumer_id", "iss", {"aud"}};
  static const char expected_json[] =
      "{\"issuer\":\"iss\",\"id\":\"id\",\"email\":\"email\",\"consumer_id\":"
      "\"consumer_id\"}";
  VerifyWriteUserInfo(expected_json, user_info);
}

TEST(EspJsonTest, DoubleQuoteTest) {
  UserInfo user_info{"id", "email \"with\" quote", "consumer_id", "iss", {}};
  static const char expected_json[] =
      "{\"issuer\":\"iss\",\"id\":\"id\",\"email\":\"email \\\"with\\\" "
      "quote\",\"consumer_id\":\"consumer_id\"}";
  VerifyWriteUserInfo(expected_json, user_info);
}

TEST(EspJsonTest, SingleQuoteTest) {
  UserInfo user_info{"id", "email 'with' quote", "consumer_id", "iss", {}};
  static const char expected_json[] =
      "{\"issuer\":\"iss\",\"id\":\"id\",\"email\":\"email 'with' "
      "quote\",\"consumer_id\":\"consumer_id\"}";
  VerifyWriteUserInfo(expected_json, user_info);
}

TEST(EspJsonTest, SlashTest) {
  UserInfo user_info{"id", "email \\with\\ quote", "consumer_id", "iss", {}};
  static const char expected_json[] =
      "{\"issuer\":\"iss\",\"id\":\"id\",\"email\":\"email \\\\with\\\\ "
      "quote\",\"consumer_id\":\"consumer_id\"}";
  VerifyWriteUserInfo(expected_json, user_info);
}

}  // namespace auth
}  // namespace api_manager
}  // namespace google
