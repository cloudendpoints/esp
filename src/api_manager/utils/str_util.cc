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

#include <sstream>

#include "src/api_manager/utils/str_util.h"

namespace google {
namespace api_manager {
namespace utils {

void Split(const std::string &s, char delim, std::vector<std::string> *elems) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    elems->push_back(item);
  }
}

const std::string Trim(std::string &str) {
  str.erase(0, str.find_first_not_of(' '));  // heading spaces
  str.erase(str.find_last_not_of(' ') + 1);  // tailing spaces
  return str;
}

}  // namespace utils
}  // namespace api_manager
}  // namespace google
