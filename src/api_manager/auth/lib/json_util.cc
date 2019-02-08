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
#include "src/api_manager/auth/lib/json_util.h"
#include <stddef.h>
#include <string.h>
#include "src/api_manager/utils/url_util.h"

extern "C" {
#include "grpc/support/log.h"
}

namespace google {
namespace api_manager {
namespace auth {

namespace {

// Delimiter of the jwt payloads
const char kJwtPayloadsDelimeter = '.';

bool isNullOrEmpty(const char *str) { return str == nullptr || *str == '\0'; }

}  // namespace

const grpc_json *GetProperty(const grpc_json *json, const char *key) {
  if (json == nullptr || key == nullptr) {
    return nullptr;
  }
  const grpc_json *cur;
  for (cur = json->child; cur != nullptr; cur = cur->next) {
    if (strcmp(cur->key, key) == 0) return cur;
  }
  return nullptr;
}

const char *GetPropertyValue(const grpc_json *json, const char *key,
                             grpc_json_type type) {
  const grpc_json *cur = GetProperty(json, key);
  if (cur != nullptr) {
    if (cur->type != type) {
      gpr_log(GPR_ERROR, "Unexpected type of a %s field [%s]: %d", key,
              cur->value, type);
      return nullptr;
    }
    return cur->value;
  }
  return nullptr;
}

const std::string GetPrimitiveFieldValue(const std::string &json,
                                         const std::string &payload_path) {
  char *json_copy = strdup(json.c_str());
  grpc_json *property_json =
      grpc_json_parse_string_with_len(json_copy, strlen(json_copy));
  std::vector<std::string> path_field;
  std::string s;
  utils::Split(payload_path, kJwtPayloadsDelimeter, &path_field);
  for (const auto &path : path_field) {
    const grpc_json *next = GetProperty(property_json, path.c_str());
    if (next) {
      *property_json = *next;
    } else {
      // Not found the corresponding jwt payload.
      property_json = nullptr;
      return "";
    }
  }

  if (property_json) {
    switch (property_json->type) {
      case GRPC_JSON_STRING:
      case GRPC_JSON_NUMBER:
        s += property_json->value;
        break;
      case GRPC_JSON_TRUE:
        s += "true";
        break;
      case GRPC_JSON_FALSE:
        s += "false";
        break;
      default:
        return "";
    }
  }
  return s;
}

const char *GetStringValue(const grpc_json *json, const char *key) {
  return GetPropertyValue(json, key, GRPC_JSON_STRING);
}

const char *GetNumberValue(const grpc_json *json, const char *key) {
  return GetPropertyValue(json, key, GRPC_JSON_NUMBER);
}

void FillChild(grpc_json *child, grpc_json *brother, grpc_json *parent,
               const char *key, const char *value, grpc_json_type type) {
  if (isNullOrEmpty(key) || isNullOrEmpty(value)) {
    return;
  }

  memset(child, 0, sizeof(grpc_json));

  if (brother) brother->next = child;
  if (!parent->child) parent->child = child;

  child->parent = parent;
  child->key = key;
  child->value = value;
  child->type = type;
}

}  // namespace auth
}  // namespace api_manager
}  // namespace google
