// Copyright (C) Endpoints Server Proxy Authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "src/api_manager/method_impl.h"

#include <sstream>

using std::map;
using std::set;
using std::string;
using std::stringstream;

namespace google {
namespace api_manager {

// The name for api key in system parameter from service config.
const char api_key_parameter_name[] = "api_key";

MethodInfoImpl::MethodInfoImpl(const string &name)
    : name_(name),
      auth_(false),
      allow_unregistered_calls_(false),
      api_key_http_headers_(nullptr),
      api_key_url_query_parameters_(nullptr),
      request_streaming_(false),
      response_streaming_(false) {}

void MethodInfoImpl::addAudiencesForIssuer(const string &issuer,
                                           const string &audiences_list) {
  if (issuer.empty()) {
    return;
  }
  set<string> &audiences = issuer_audiences_map_[issuer];
  stringstream ss(audiences_list);
  string audience;
  // Audience list is comma-delimited.
  while (getline(ss, audience, ',')) {
    if (!audience.empty()) {  // Only adds non-empty audience.
      audiences.insert(audience);
    }
  }
}

bool MethodInfoImpl::isAudienceAllowed(
    const string &issuer, const std::set<string> &jwt_audiences) const {
  if (issuer.empty() || jwt_audiences.empty() || !isIssuerAllowed(issuer)) {
    return false;
  }
  const set<string> &audiences = issuer_audiences_map_.at(issuer);
  for (const auto &it : jwt_audiences) {
    if (audiences.find(it) != audiences.end()) {
      return true;
    }
  }
  return false;
}

void MethodInfoImpl::process_system_parameters() {
  api_key_http_headers_ = http_header_parameters(api_key_parameter_name);
  api_key_url_query_parameters_ = url_query_parameters(api_key_parameter_name);
}

}  // namespace api_manager
}  // namespace google
