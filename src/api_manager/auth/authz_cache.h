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
#ifndef API_MANAGER_AUTH_AUTHZ_CACHE_H_
#define API_MANAGER_AUTH_AUTHZ_CACHE_H_

#include <chrono>
#include <string>
#include "utils/simple_lru_cache_inl.h"
#include "utils/md5.h"

namespace google {
namespace api_manager {
namespace auth {

// A authz_cache value struct.
struct AuthzValue {
  // Authorization result.
  bool if_success;
  // Expiration time of the cache entry. This is the minimum of "exp" field in
  // the JWT and [the time this cache entry is added + CacheEntryTTL]
  std::chrono::system_clock::time_point exp;
};

// A local cache to expedite the authorization process. The key of the cache is
// the hash of the concatenation of JWT auth token, request path, and request HTTP
// method. The value is of type AuthzValue.
class AuthzCache : public ::google::service_control_client::SimpleLRUCache<std::string, AuthzValue> {
  public:
    AuthzCache();
    ~AuthzCache();
    // This method is used to insert cache entry.
    void Add(const std::string& auth_token, const std::string& request_path,
                           const std::string& request_HTTP_method, const bool if_success,
                           const std::chrono::system_clock::time_point& token_exp,
                           const std::chrono::system_clock::time_point& now);
    // This method is used to do cache lookup.
    bool Lookup(const std::string& auth_token, const std::string& request_path,
                                              const std::string& request_HTTP_method, AuthzValue* value);
    // This method is used to delete a cache entry.
    void Delete(const std::string& auth_token, const std::string& request_path,
                                              const std::string& request_HTTP_method);
  private:
    // This method is used to generate cache key.
    std::string ComposeAuthzCacheKey(const std::string& auth_token, const std::string& request_path,
                                              const std::string& request_HTTP_method);
};

}  // namespace auth
}  // namespace api_manager
}  // namespace google

#endif // API_MANAGER_AUTH_AUTHZ_CACHE_H_


