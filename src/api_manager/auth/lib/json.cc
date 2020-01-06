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

#include <cstring>
#include <string>

#include "src/api_manager/auth/lib/json_util.h"

namespace google {
namespace api_manager {
namespace auth {

char *WriteUserInfoToJson(const UserInfo &user_info) {
  grpc_json json_top;
  memset(&json_top, 0, sizeof(json_top));
  json_top.type = GRPC_JSON_OBJECT;
  grpc_json *prev_child = nullptr;

  grpc_json json_all_claims;
  prev_child = FillChild(&json_all_claims, prev_child, &json_top, "claims",
                         user_info.claims.c_str(), GRPC_JSON_STRING);

  grpc_json json_issuer;
  prev_child = FillChild(&json_issuer, prev_child, &json_top, "issuer",
                         user_info.issuer.c_str(), GRPC_JSON_STRING);

  grpc_json json_id;
  prev_child = FillChild(&json_id, prev_child, &json_top, "id",
                         user_info.id.c_str(), GRPC_JSON_STRING);

  grpc_json json_email;
  prev_child = FillChild(&json_email, prev_child, &json_top, "email",
                         user_info.email.c_str(), GRPC_JSON_STRING);

  grpc_json json_consumer_id;
  prev_child =
      FillChild(&json_consumer_id, prev_child, &json_top, "consumer_id",
                user_info.consumer_id.c_str(), GRPC_JSON_STRING);

  grpc_json json_audiences;
  grpc_json json_audience_array[user_info.audiences.size()];
  CreateGrpcJsonArrayByStringSet(
      user_info.audiences, json_audience_array, prev_child, &json_top,
      "audiences", &json_audiences);

  return grpc_json_dump_to_string(&json_top, 0);
}

} // namespace auth
} // namespace api_manager
} // namespace google
