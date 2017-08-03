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
#include "gtest/gtest.h"

namespace google {
namespace api_manager {
namespace auth {
namespace {

const std::string kPath = "/path/to/resource";
const std::string kPath1 = "path/to/resources";
const std::string kHTTPMethod = "GET";
const std::string kAuthToken =
    "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI2Mjg2NDU3NDE4ODEtbm9hYml1M"
    "jNmNWE4bThvdmQ4dWN2Njk4bGo3OHZ2MGxAZGV2ZWxvcGVyLmdzZXJ2aWNlYWNjb3VudC5jb20"
    "iLCJzdWIiOiI2Mjg2NDU3NDE4ODEtbm9hYml1MjNmNWE4bThvdmQ4dWN2Njk4bGo3OHZ2MGxAZ"
    "GV2ZWxvcGVyLmdzZXJ2aWNlYWNjb3VudC5jb20iLCJhdWQiOiJodHRwOi8vbXlzZXJ2aWNlLmN"
    "vbS9teWFwaSJ9.gq_4ucjddQDjYK5FJr_kXmMo2fgSEB6Js1zopcQLVpCKFDNb-TQ97go0wuk5"
    "_vlSp_8I2ImrcdwYbAKqYCzcdyBXkAYoHCGgmY-v6MwZFUvrIaDzR_M3rmY8sQ8cdN3MN6ZRbB"
    "6opHwDP1lUEx4bZn_ZBjJMPgqbIqGmhoT1UpfPF6P1eI7sXYru-4KVna0STOynLl3d7JYb7E-8"
    "ifcjUJLhat8JR4zR8i4-zWjn6d6j_NI7ZvMROnao77D9YyhXv56zfsXRatKzzYtxPlQMz4AjP-"
    "bUHfbHmhiIOOAeEKFuIVUAwM17j54M6VQ5jnAabY5O-ermLfwPiXvNt2L2SA==";
const int kAuthzCacheTimeout = 300;

class TestAuthzCache : public ::testing::Test {
 public:
  virtual void SetUp() {}
  void AuthzCacheTest(bool token_exp_earlier);
 private:
  AuthzCache cache_;
};

// Test authz_cache functionalities.
void TestAuthzCache::AuthzCacheTest(bool token_exp_earlier) {
  AuthzValue val;
  // Initially there is no entry stored in cache.
  ASSERT_FALSE(cache_.Lookup(kAuthToken, kPath, kHTTPMethod, &val));
  // Two scenarios.
  // (1) Token expires sooner than cache entry does.
  // (2) Cache entry expires sooner than token does.
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  std::chrono::system_clock::time_point token_exp;
  if (token_exp_earlier) {
    token_exp = now + std::chrono::seconds(kAuthzCacheTimeout - 1);
  } else {
    token_exp = now + std::chrono::seconds(kAuthzCacheTimeout + 1);
  }
  // After inserting the cache entry, try to verify both scenarios metioned
  // above.
  cache_.Add(kAuthToken, kPath, kHTTPMethod, true, token_exp, now);
  ASSERT_TRUE(cache_.Lookup(kAuthToken, kPath, kHTTPMethod, &val));
  // This verifies the key-generating algorihtm (ComposeAuthzCacheKey) is correct,
  // for different key will not lead to a cache hit if this key is not cached.
  ASSERT_FALSE(cache_.Lookup(kAuthToken, kPath1, kHTTPMethod, &val));
  ASSERT_EQ(val.if_success, true);
  if (token_exp_earlier) {
    ASSERT_EQ(val.exp, token_exp);
  } else {
    ASSERT_EQ(val.exp, now + std::chrono::seconds(kAuthzCacheTimeout));
  }
  // Check the existence of cache entry after removal.
  cache_.Delete(kAuthToken, kPath, kHTTPMethod);
  ASSERT_FALSE(cache_.Lookup(kAuthToken, kPath, kHTTPMethod, &val));
}

TEST_F(TestAuthzCache, Unittest) {
  // First scenario.
  AuthzCacheTest(true);
  // Second scenario.
  AuthzCacheTest(false);
}

}  // namespace
}  // namespace auth
}  // namespace api_manager
}  // namespace google
