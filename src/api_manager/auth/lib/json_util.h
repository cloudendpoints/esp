/* Copyright (C) Extensible Service Proxy Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef API_MANAGER_AUTH_LIB_JSON_UTIL_H_
#define API_MANAGER_AUTH_LIB_JSON_UTIL_H_

// This header file is for auth library internal use only
// since it directly includes a grpc header file.
// A public header file should not include any grpc header files.

#include <set>
#include "src/api_manager/auth/lib/grpc_internals.h"

namespace google {
namespace api_manager {
namespace auth {

// Gets given JSON property by key name.
const grpc_json *GetProperty(const grpc_json *json, const char *key);

// Gets the primitive value of the json with given path, separated by ".".
bool GetPrimitiveFieldValue(const std::string &json,
                            const std::string &payload_path,
                            std::string *payload_value);

// Gets string value by key or nullptr if no such key or property is not string
// type.
const char *GetStringValue(const grpc_json *json, const char *key);

// Gets a value of a number property with a given key, or nullptr if no such key
// exists or the property is property is not number type.
const char *GetNumberValue(const grpc_json *json, const char *key);

// Fill grpc_child with key, value and type, and setup links from/to
// brother/parents.
grpc_json *FillChild(grpc_json *child, grpc_json *brother, grpc_json *parent,
                     const char *key, const char *value, grpc_json_type type);

// Create a grpc json array by a set of string
grpc_json *CreateGrpcJsonArray(const std::set<std::string> &strSet,
                               grpc_json *brother, grpc_json *parent,
                               const char *key, grpc_json *json_array,
                               grpc_json *array_elem);

}  // namespace auth
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_AUTH_LIB_JSON_UTIL_H_
