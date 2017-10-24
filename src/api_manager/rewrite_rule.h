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
#ifndef API_MANAGER_REWRITE_RULE_H_
#define API_MANAGER_REWRITE_RULE_H_

#include <cctype>
#include <iostream>
#include <regex>
#include <sstream>
#include <vector>

#include "include/api_manager/api_manager.h"
#include "include/api_manager/utils/status.h"
#include "src/api_manager/config_manager.h"
#include "src/api_manager/context/global_context.h"
#include "src/api_manager/context/service_context.h"
#include "src/api_manager/service_control/interface.h"
#include "src/api_manager/weighted_selector.h"

namespace google {
namespace api_manager {

class RewriteRule {
 public:
  enum ReplacementPartType { TEXT, REPLACEMENT };

  struct ReplacementSegment {
    ReplacementPartType type;
    int index;
    std::string text;

    ReplacementSegment(ReplacementPartType type, int index, std::string text)
        : type(type), index(index), text(text) {}
  };

  RewriteRule(std::string regex, std::string replacement,
              ApiManagerEnvInterface *env, bool debug_mode);

  virtual ~RewriteRule() {}

  bool Check(const std::string &uri, std::string *destination);

 private:
  std::string regex_pattern_;
  std::unique_ptr<std::regex> regex_;
  std::string replacement_;
  std::vector<ReplacementSegment> replacement_parts_;
  ApiManagerEnvInterface *env_;
  bool debug_mode_;
};

}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_QUOTA_CONTROL_H_
