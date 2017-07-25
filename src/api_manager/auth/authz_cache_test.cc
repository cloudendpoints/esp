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
#include <memory>
#include "gtest/gtest.h"
#include "utils/md5.h"
#include <iostream>

using std::chrono::system_clock;

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
  virtual void SetUp() { cache_.reset(new AuthzCache()); }

  std::unique_ptr<AuthzCache> cache_;
};
// Test ComposeAuthCacheKey function in AuthzCache class.
void TestComposeAuthCacheKey(AuthzCache *cache) {
  // String concatenation with same strings resulting in same key.
  std::string *key = cache->ComposeAuthzCacheKey(kAuthToken, kPath, kHTTPMethod);
  ASSERT_EQ(key->length(), 16);
  google::service_control_client::MD5 hasher;
  hasher.Update(kAuthToken+kPath+kHTTPMethod);
  ASSERT_EQ(*key, hasher.Digest());

  // String concatenation with different strings resulting in different keys.
  std::string *key1 =  cache->ComposeAuthzCacheKey(kAuthToken, kPath1, kHTTPMethod);
  ASSERT_NE(*key, *key1);

}

// Test Insert function in AuthzCache class.
void TestInsert(AuthzCache *cache, bool token_exp_earlier) {

  std::string *key = cache->ComposeAuthzCacheKey(kAuthToken, kPath, kHTTPMethod);
  // Initially there is no entry stored in cache.
  ASSERT_EQ(nullptr, cache->Lookup(*key));
  // Two scenarios.
  // (1) Token expires sooner than cache entry does.
  // (2) Cache entry expires sooner than token does.
  system_clock::time_point now = system_clock::now();
  system_clock::time_point token_exp;
  if (token_exp_earlier) {
    token_exp = now + std::chrono::seconds(kAuthzCacheTimeout - 1);
  } else {
    token_exp = now + std::chrono::seconds(kAuthzCacheTimeout + 1);
  }
  // After inserting the cache entry, try to verify both scenarios metioned
  // above.
  cache->Insert(*key, true, token_exp, now);
  AuthzValue* val = cache->Lookup(*key);
  ASSERT_NE(nullptr, val);
  ASSERT_EQ(val->if_success, true);
  if (token_exp_earlier) {
    ASSERT_EQ(val->exp, token_exp);
  } else {
    ASSERT_EQ(val->exp, now + std::chrono::seconds(kAuthzCacheTimeout));
  }
  // Check the existence of cache entry after removal.
  cache->Release(*key, val);
  cache->Remove(*key);
  ASSERT_EQ(nullptr, cache->Lookup(*key));
}



TEST_F(TestAuthzCache, ComposeKeyTest) {
  TestComposeAuthCacheKey(cache_.get());
}


TEST_F(TestAuthzCache, InsertTest) {
  // First scenario.
  TestInsert(cache_.get(), true);

  // Second scenario.
  TestInsert(cache_.get(), false);
}

}  // namespace

}  // namespace auth
}  // namespace api_manager
}  // namespace google
