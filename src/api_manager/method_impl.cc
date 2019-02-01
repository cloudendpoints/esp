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
#include "src/api_manager/method_impl.h"
#include "src/api_manager/utils/url_util.h"

#include <sstream>

using std::map;
using std::set;
using std::string;
using std::stringstream;

namespace google {
namespace api_manager {

// The name for api key in system parameter from service config.
const char api_key_parameter_name[] = "api_key";

MethodInfoImpl::MethodInfoImpl(const string &name, const string &api_name,
                               const string &api_version)
    : name_(name),
      api_name_(api_name),
      api_version_(api_version),
      auth_(false),
      allow_unregistered_calls_(false),
      skip_service_control_(false),
      api_key_http_headers_(nullptr),
      api_key_url_query_parameters_(nullptr),
      request_streaming_(false),
      response_streaming_(false) {}

void MethodInfoImpl::addAuthProvider(const std::string &issuer,
                                     const string &audiences_list,
                                     const std::string &authorization_url) {
  if (issuer.empty()) {
    return;
  }
  std::string iss = utils::GetUrlContent(issuer);
  if (iss.empty()) {
    return;
  }
  AuthProvider &provider = issuer_provider_map_[iss];
  stringstream ss(audiences_list);
  string audience;
  // Audience list is comma-delimited.
  while (getline(ss, audience, ',')) {
    if (!audience.empty()) {  // Only adds non-empty audience.
      std::string aud = utils::GetUrlContent(audience);
      if (!aud.empty()) {
        provider.audiences.insert(aud);
      }
    }
  }
  provider.authorization_url = authorization_url;
}

bool MethodInfoImpl::isIssuerAllowed(const std::string &issuer) const {
  return !issuer.empty() &&
         issuer_provider_map_.find(issuer) != issuer_provider_map_.end();
}

bool MethodInfoImpl::isAudienceAllowed(
    const string &issuer, const std::set<string> &jwt_audiences) const {
  if (issuer.empty() || jwt_audiences.empty() || !isIssuerAllowed(issuer)) {
    return false;
  }
  const AuthProvider &provider = issuer_provider_map_.at(issuer);
  for (const auto &it : jwt_audiences) {
    if (provider.audiences.find(it) != provider.audiences.end()) {
      return true;
    }
  }
  return false;
}

const std::string &MethodInfoImpl::authorization_url_by_issuer(
    const std::string &issuer) const {
  const auto &it = issuer_provider_map_.find(issuer);
  if (it != issuer_provider_map_.end()) {
    return it->second.authorization_url;
  } else {
    static std::string empty;
    return empty;
  }
}

const std::string &MethodInfoImpl::first_authorization_url() const {
  for (const auto &it : issuer_provider_map_) {
    if (!it.second.authorization_url.empty()) {
      return it.second.authorization_url;
    }
  }
  static std::string empty;
  return empty;
}

void MethodInfoImpl::process_backend_rule(
    const ::google::api::BackendRule &rule) {
  backend_address_ = rule.address();
  backend_path_translation_ = rule.path_translation();
  backend_jwt_audience_ = rule.jwt_audience();

  if (backend_path_translation_ ==
      ::google::api::BackendRule_PathTranslation_CONSTANT_ADDRESS) {
    // for CONSTANT ADDRESS case, needs to split the rule.address into
    // address and path for a full URL. If it is not a full URL, leave
    // backend_address_ same as rule.address.
    string::size_type i = backend_address_.find("/");
    int j;
    for (j = 0; j < 2; ++j) {
      i = backend_address_.find("/", i + 1);
    }
    if (i != string::npos) {
      backend_path_ = backend_address_.substr(i);
      backend_address_ = backend_address_.substr(0, i);
    }
    return;
  }
  // Strip the last "/", in case the address is mis-configured.
  if (backend_path_translation_ ==
          ::google::api::BackendRule_PathTranslation_APPEND_PATH_TO_ADDRESS &&
      backend_address_.back() == '/') {
    backend_address_ = backend_address_.substr(0, backend_address_.size() - 1);
  }
}

void MethodInfoImpl::process_system_parameters() {
  api_key_http_headers_ = http_header_parameters(api_key_parameter_name);
  api_key_url_query_parameters_ = url_query_parameters(api_key_parameter_name);
}

void MethodInfoImpl::ProcessSystemQueryParameterNames() {
  for (const auto &param : url_query_parameters_) {
    for (const auto &name : param.second) {
      system_query_parameter_names_.insert(name);
    }
  }

  if (!api_key_http_headers_ && !api_key_url_query_parameters_) {
    // Adding the default api_key url query parameters
    system_query_parameter_names_.insert("key");
    system_query_parameter_names_.insert("api_key");
  }
}

}  // namespace api_manager
}  // namespace google
