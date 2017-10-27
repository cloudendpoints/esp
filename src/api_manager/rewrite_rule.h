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
#include <sstream>
#include <vector>

#include "pcre.h"

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
  RewriteRule(std::string regex, std::string replacement,
              ApiManagerEnvInterface *env);

  virtual ~RewriteRule();

  // Returns true if the request uri is matched to regex pattern.
  // If request uri and patter match, destination will have the replace uri.
  // Otherwise returns false.
  bool Check(const char *uri, size_t uri_len, std::string *destination,
             bool debug_mode);

  // Validate rewrite rule
  // Return true if format is correct. Otherwise returns false
  static bool ValidateRewriteRule(const std::string &rule,
                                  std::string *error_msg);

 private:
  // Parts matched to "(\$[0-9]+)" are REPLACEMENT, others are TEXT
  enum ReplacementPartType { TEXT, REPLACEMENT };

  // segment for replacement pattern
  struct ReplacementSegment {
    // Replacement part type
    ReplacementPartType type;
    // For REPLACEMENT type
    // matched pattern index for the segment
    int index;
    // For TEXT type
    // text will be appended
    std::string text;

    ReplacementSegment(ReplacementPartType type, int index, std::string text)
        : type(type), index(index), text(text) {}
  };

  // original pcre regular expression pattern
  std::string regex_pattern_;

  // pcre compiled and studied pointers
  pcre *regex_compiled_;
  pcre_extra *regex_extra_;

  // original replacement string
  std::string replacement_;
  // parsed replacement string
  std::vector<ReplacementSegment> replacement_parts_;

  // ApiManager environment for error logging
  ApiManagerEnvInterface *env_;
};

}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_QUOTA_CONTROL_H_
