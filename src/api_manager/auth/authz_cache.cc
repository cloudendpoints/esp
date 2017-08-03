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
#include "src/api_manager/auth/authz_cache.h"

using ::google::service_control_client::SimpleLRUCache;

namespace google {
namespace api_manager {
namespace auth {

namespace {
// The maximum lifetime of a cache entry. Unit: seconds.
const int kAuthzCacheTimeout = 300;
// The number of entries in authz cache.
const int kAuthzCacheSize = 100;
} // namespace

  AuthzCache::AuthzCache() : SimpleLRUCache<std::string, AuthzValue>(kAuthzCacheSize) {}

  AuthzCache::~AuthzCache() { Clear(); }

  void AuthzCache::Add(const std::string& auth_token, const std::string& request_path,
                           const std::string& request_HTTP_method, const bool if_success,
                           const std::chrono::system_clock::time_point& token_exp,
                           const std::chrono::system_clock::time_point& now) {
    std::string cache_key = ComposeAuthzCacheKey(auth_token, request_path, request_HTTP_method);
    AuthzValue* newval = new AuthzValue();
    newval->if_success = if_success;
    newval->exp = std::min(token_exp, now + std::chrono::seconds(kAuthzCacheTimeout));
    SimpleLRUCache::Insert(cache_key, newval, 1);
  }

  std::string AuthzCache::ComposeAuthzCacheKey(const std::string& auth_token, const std::string& request_path,
                                              const std::string& request_HTTP_method) {
    google::service_control_client::MD5 hasher;
    hasher.Update(auth_token);
    hasher.Update(request_path);
    hasher.Update(request_HTTP_method);
    return hasher.Digest();
  }

  bool AuthzCache::Lookup(const std::string& auth_token, const std::string& request_path,
                                              const std::string& request_HTTP_method, AuthzValue* value){
    std::string cache_key = ComposeAuthzCacheKey(auth_token, request_path, request_HTTP_method);
    // ScopedLookup automatically releases the possessed value (returned value pointer by lookup.value())
    // when the lookup instance goes out of scope.
    ScopedLookup lookup(this, cache_key);
    bool ret = lookup.Found();
    if (ret) {
      value->if_success = lookup.value()->if_success;
      value->exp = lookup.value()->exp;
    }
    return ret;
  }

  void AuthzCache::Delete(const std::string& auth_token, const std::string& request_path,
                                              const std::string& request_HTTP_method) {
    std::string cache_key = ComposeAuthzCacheKey(auth_token, request_path, request_HTTP_method);
    Remove(cache_key);
  }

}  // namespace auth
}  // namespace api_manager
}  // namespace google
