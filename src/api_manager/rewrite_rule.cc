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
#include "rewrite_rule.h"

namespace google {
namespace api_manager {

namespace {

std::string kEspRewriteTitle = "esp_rewrite";
}

RewriteRule::RewriteRule(std::string regex, std::string replacement,
                         ApiManagerEnvInterface *env, bool debug_mode)
    : regex_pattern_(regex),
      replacement_(replacement),
      env_(env),
      debug_mode_(debug_mode) {
  try {
    regex_.reset(new std::regex(regex, std::regex::extended));
  } catch (const std::regex_error &e) {
    env_->LogError("Invalid rewrite rule: \"" + regex + "\", error: " +
                   e.what());

    regex_.reset();
    return;
  }

  std::string segment;
  ReplacementPartType status = ReplacementPartType::TEXT;

  for (std::string::iterator it = replacement.begin(); it != replacement.end();
       ++it) {
    switch (status) {
      case ReplacementPartType::TEXT:
        if (*it == '$') {
          if (segment.length() > 0) {
            replacement_parts_.push_back(
                ReplacementSegment(status, -1, segment));
          }
          segment.clear();
          status = ReplacementPartType::REPLACEMENT;
        } else {
          segment.append(std::string(1, *it));
        }
        break;
      case ReplacementPartType::REPLACEMENT:
        if (isdigit(*it)) {
          segment.append(std::string(1, *it));
        } else {
          if (segment.length() > 0) {
            replacement_parts_.push_back(
                ReplacementSegment(status, std::stoi(segment), ""));
          } else {
            replacement_parts_.push_back(
                ReplacementSegment(ReplacementPartType::TEXT, -1, "$"));
          }

          if (*it == '$') {
            segment.clear();
          } else {
            segment.assign(std::string(1, *it));
            status = ReplacementPartType::TEXT;
          }
        }
        break;
    }
  }

  if (segment.length() > 0) {
    switch (status) {
      case ReplacementPartType::TEXT:
        replacement_parts_.push_back(ReplacementSegment(status, -1, segment));
        break;
      case ReplacementPartType::REPLACEMENT:
        replacement_parts_.push_back(
            ReplacementSegment(status, std::stoi(segment), ""));
        break;
    }
  }
}

bool RewriteRule::Check(const std::string &uri, std::string *destination) {
  if (regex_.get() == nullptr) {
    env_->LogInfo("Rewrite rule was not initialized");
    return false;
  }

  try {
    std::cmatch cm;
    std::stringstream rewrite_log;

    std::regex_match(uri.c_str(), cm, *regex_.get());

    if (debug_mode_) {
      rewrite_log << kEspRewriteTitle << ": matching rule: " << regex_pattern_
                  << " " << replacement_ << std::endl;
    }

    if (cm.size() == 0) {
      if (debug_mode_) {
        rewrite_log << kEspRewriteTitle + ": request uri doesn't match with "
                    << this->regex_pattern_ << std::endl;
        env_->LogError(rewrite_log.str());
      }
      return false;
    }

    if (debug_mode_) {
      rewrite_log << kEspRewriteTitle << ": regex=" << regex_pattern_
                  << std::endl;
      rewrite_log << kEspRewriteTitle << ": request uri=" << uri << std::endl;
      for (size_t i = 0; i < cm.size(); i++) {
        rewrite_log << kEspRewriteTitle << ": $" << std::to_string(i) << ": "
                    << std::string(cm[i]) << std::endl;
      }

      rewrite_log << kEspRewriteTitle << ": replacement: " << replacement_
                  << std::endl;
    }

    std::stringstream ss;
    for (auto it = replacement_parts_.begin(); it != replacement_parts_.end();
         ++it) {
      switch (it->type) {
        case ReplacementPartType::TEXT:
          ss << it->text;
          break;
        case ReplacementPartType::REPLACEMENT:
          if (it->index >= 0 && it->index < (int)cm.size()) {
            ss << cm[it->index];
          }
          break;
      }
    }

    destination->assign(ss.str());

    if (debug_mode_) {
      rewrite_log << kEspRewriteTitle << ": destination uri: " << *destination;
      env_->LogInfo(rewrite_log.str());
    }

    return true;
  } catch (const std::regex_error &e) {
    env_->LogError("Error matching rewrite rule. error: " +
                   std::string(e.what()));
    return false;
  }
}

}  // namespace api_manager
}  // namespace google
