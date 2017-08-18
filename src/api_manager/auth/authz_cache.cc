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

namespace google {
namespace api_manager {
namespace auth {

namespace {
// The maximum lifetime of a cache entry. Unit: seconds.
const int kAuthzCacheTimeout = 300;
// The number of entries in authz cache.
const int kAuthzCacheSize = 200;
}  // namespace

AuthzCache::AuthzCache() : cache_(kAuthzCacheSize) {}

AuthzCache::~AuthzCache() { cache_.Clear(); }

void AuthzCache::Add(const std::string& cache_key, const bool if_success,
                     const std::chrono::system_clock::time_point& now) {
  AuthzValue* newval = new AuthzValue();
  newval->if_success = if_success;
  newval->exp = now + std::chrono::seconds(kAuthzCacheTimeout);
  cache_.Insert(cache_key, newval, 1);
}

bool AuthzCache::Lookup(const std::string& cache_key,
                        const std::chrono::system_clock::time_point& now,
                        AuthzValue* value) {
  ::google::service_control_client::SimpleLRUCache<
      std::string, AuthzValue>::ScopedLookup lookup(&cache_, cache_key);
  if (!lookup.Found()) {
    return false;
  }
  AuthzValue* val = lookup.value();
  if (now > val->exp) {
    cache_.Remove(cache_key);
    return false;
  }
  *value = *val;
  return true;
}

std::string AuthzCache::ComposeAuthzCacheKey(
    const std::string& auth_token, const std::string& request_path,
    const std::string& request_HTTP_method) {
  google::service_control_client::MD5 hasher;
  hasher.Update(auth_token);
  hasher.Update(request_path);
  hasher.Update(request_HTTP_method);
  return hasher.Digest();
}

int AuthzCache::NumberOfEntries() { return cache_.Entries(); }

}  // namespace auth
}  // namespace api_manager
}  // namespace google
