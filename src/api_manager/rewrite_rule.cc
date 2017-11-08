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

// module title
const std::string kEspRewriteTitle = "esp_rewrite";

// Maximum number of matching segment for regular expression
const int kMaxRegexMathCount = 100;

// Internal memory allocation function
void *esp_regex_malloc(size_t size) { return malloc(size); }

// Internal memory free function
void esp_regex_free(void *p) { free(p); }

// Backup and override pcre_malloc function pointer which was overridden by
// nginx. Since nginx is single thread model, it is safe to override and restore
class PcreMemoryFunctionOverride {
 public:
  PcreMemoryFunctionOverride() {
    origin_pcre_malloc_ = pcre_malloc;
    origin_pcre_free_ = pcre_free;
    pcre_malloc = esp_regex_malloc;
    pcre_free = esp_regex_free;
  }

  virtual ~PcreMemoryFunctionOverride() {
    // Restore saved function pointer
    pcre_malloc = origin_pcre_malloc_;
    pcre_free = origin_pcre_free_;
  }

 private:
  // backup for pcre_malloc
  void *(*origin_pcre_malloc_)(size_t);
  // backup for pcre_free
  void (*origin_pcre_free_)(void *);
};

}  // namespace

bool RewriteRule::ValidateRewriteRule(const std::string &rule,
                                      std::string *error_msg) {
  PcreMemoryFunctionOverride scoped_override;

  std::stringstream ss(rule);
  std::istream_iterator<std::string> begin(ss);
  std::istream_iterator<std::string> end;
  std::vector<std::string> parts(begin, end);

  if (parts.size() != 2) {
    error_msg->assign("Invalid rewrite rule format(pattern replacement): \"" +
                      rule + "\"");
    return false;
  }

  if (strncmp(parts[1].c_str(), "http://", strlen("http://")) == 0 ||
      strncmp(parts[1].c_str(), "https://", strlen("https://")) == 0) {
    error_msg->assign(
        "Replacement starts with either \"http://\" or \"https://\": is not "
        "allowed: \"" +
        rule + "\"");
    return false;
  }

  pcre *regex_compiled;
  pcre_extra *regex_extra;

  const char *pcre_error_str;
  int pcre_error_offset;
  regex_compiled = pcre_compile(parts[0].c_str(), 0, &pcre_error_str,
                                &pcre_error_offset, NULL);
  if (regex_compiled == NULL) {
    error_msg->assign("Invalid regular expression in the rewrite rule: \"" +
                      rule + "\"");

    return false;
  }

  regex_extra = pcre_study(regex_compiled, 0, &pcre_error_str);
  if (pcre_error_str != NULL) {
    error_msg->assign("Invalid regular expression in the rewrite rule: \"" +
                      rule + "\"");

    pcre_free(regex_compiled);
    return false;
  }

  pcre_free(regex_compiled);

  if (regex_extra != NULL) {
#ifdef PCRE_CONFIG_JIT
    pcre_free_study(regex_extra);
#else
    pcre_free(pcreExtra);
#endif
  }

  return true;
}

RewriteRule::RewriteRule(std::string regex, std::string replacement,
                         ApiManagerEnvInterface *env)
    : regex_pattern_(regex),
      regex_compiled_(NULL),
      regex_extra_(NULL),
      replacement_(replacement),
      env_(env) {
  PcreMemoryFunctionOverride scoped_override;

  const char *pcre_error_str;
  int pcre_error_offset;

  regex_compiled_ = pcre_compile(regex_pattern_.c_str(), 0, &pcre_error_str,
                                 &pcre_error_offset, NULL);
  if (regex_compiled_ == NULL) {
    env_->LogError("Invalid rewrite rule: \"" + regex_pattern_ + "\", error: " +
                   std::string(pcre_error_str));
    return;
  }

  regex_extra_ = pcre_study(regex_compiled_, 0, &pcre_error_str);
  if (pcre_error_str != NULL) {
    env_->LogError("Invalid rewrite rule: \"" + regex_pattern_ + "\", error: " +
                   std::string(pcre_error_str));

    pcre_free(regex_compiled_);
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
      default:
        env_->LogError("Unexpected status: " + status);
        break;
    }
  }
}

RewriteRule::~RewriteRule() {
  PcreMemoryFunctionOverride scoped_override;

  pcre_free(regex_compiled_);

  if (regex_extra_ != NULL) {
#ifdef PCRE_CONFIG_JIT
    pcre_free_study(regex_extra_);
#else
    pcre_free(pcreExtra);
#endif
  }
}

bool RewriteRule::Check(const char *uri, size_t uri_len,
                        std::string *destination, bool debug_mode) {
  if (regex_compiled_ == NULL) {
    if (debug_mode) {
      env_->LogInfo("Rewrite rule was not initialized");
    }
    return false;
  }

  PcreMemoryFunctionOverride scoped_override;

  std::stringstream rewrite_log;

  int sub_str_vec[kMaxRegexMathCount];
  int pcre_exec_ret = pcre_exec(regex_compiled_, regex_extra_, uri,
                                uri_len,  // length of string
                                0,        // Start looking at this point
                                0,        // OPTIONS
                                sub_str_vec,
                                kMaxRegexMathCount);  // Length of sub_str_vec

  if (pcre_exec_ret < 0) {  // Something bad happened..
    if (debug_mode) {
      std::string msg;

      switch (pcre_exec_ret) {
        case PCRE_ERROR_NOMATCH:
          msg.assign("String did not match the pattern");
          break;
        case PCRE_ERROR_NULL:
          msg.assign("Something was null");
          break;
        case PCRE_ERROR_BADOPTION:
          msg.assign("A bad option was passed");
          break;
        case PCRE_ERROR_BADMAGIC:
          msg.assign("Magic number bad (compiled re corrupt?)");
          break;
        case PCRE_ERROR_UNKNOWN_NODE:
          msg.assign("Something kooky in the compiled re");
          break;
        case PCRE_ERROR_NOMEMORY:
          msg.assign("Ran out of memory");
          break;
        default:
          msg.assign("Unknown error");
          break;
      }

      rewrite_log << kEspRewriteTitle << ": matching rule: " << regex_pattern_
                  << ", request uri: " << uri << ", error: " << msg
                  << std::endl;
      env_->LogInfo(rewrite_log.str());
    }

    return false;
  }

  if (debug_mode) {
    rewrite_log << kEspRewriteTitle << ": matching rule: " << regex_pattern_
                << " " << this->replacement_ << std::endl;
  }

  // At this point, pcre_exec_ret contains the number of substring matches found
  if (pcre_exec_ret == 0) {
    rewrite_log << kEspRewriteTitle
                << ": too many substrings were found to fit in sub_str_vec!"
                << std::endl;
    // Set pcre_exec_ret to the max number of substring matches possible.
    pcre_exec_ret = kMaxRegexMathCount / 3;
  }

  if (debug_mode) {
    rewrite_log << kEspRewriteTitle << ": regex=" << regex_pattern_
                << std::endl;
    rewrite_log << kEspRewriteTitle << ": request uri=" << uri << std::endl;

    const char *psub_str_match_str;
    for (int j = 0; j < pcre_exec_ret; j++) {
      pcre_get_substring(uri, sub_str_vec, pcre_exec_ret, j,
                         &(psub_str_match_str));

      rewrite_log << kEspRewriteTitle << ": $" << std::to_string(j) << ": "
                  << std::string(psub_str_match_str) << std::endl;
    }
    pcre_free_substring(psub_str_match_str);

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
        const char *psub_str_match_str;
        if (it->index >= 0 && it->index < pcre_exec_ret) {
          pcre_get_substring(uri, sub_str_vec, pcre_exec_ret, it->index,
                             &(psub_str_match_str));
          ss << psub_str_match_str;
          pcre_free_substring(psub_str_match_str);
        }
        break;
      default:
        // we should not get here
        env_->LogError("Unexpected type: " + it->type);
        break;
    }
  }

  destination->assign(ss.str());

  if (debug_mode) {
    rewrite_log << kEspRewriteTitle << ": destination uri: " << *destination;
    env_->LogInfo(rewrite_log.str());
  }

  return true;
}

}  // namespace api_manager
}  // namespace google
