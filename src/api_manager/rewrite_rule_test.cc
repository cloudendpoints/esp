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
#include "gtest/gtest.h"
#include "src/api_manager/api_manager_impl.h"
#include "src/api_manager/mock_api_manager_environment.h"

#include <iostream>

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {

namespace {

std::string kExpectedRewriteLog =
    R"(INFO esp_rewrite: matching rule: /api/(.*) /$1
esp_rewrite: regex=/api/(.*)
esp_rewrite: request uri=/api/shelves?key=AIzaSyCfvOENA9MbRupfKQau2X_l8NGMVWF_byI
esp_rewrite: $0: /api/shelves?key=AIzaSyCfvOENA9MbRupfKQau2X_l8NGMVWF_byI
esp_rewrite: $1: shelves?key=AIzaSyCfvOENA9MbRupfKQau2X_l8NGMVWF_byI
esp_rewrite: replacement: /$1
esp_rewrite: destination uri: /shelves?key=AIzaSyCfvOENA9MbRupfKQau2X_l8NGMVWF_byI)";

std::string kExpectedRewriteErrorLog =
    "ERROR Invalid rewrite rule: \"/api/(.\\*\\)\", error: missing )";
std::string kExpectedNotInitializedLog =
    "INFO Rewrite rule was not initialized";

// Simulate periodic timer event on creation
class MockPeriodicTimer : public PeriodicTimer {
 public:
  MockPeriodicTimer() {}
  MockPeriodicTimer(std::function<void()> continuation)
      : continuation_(continuation) {
    continuation_();
  }

  virtual ~MockPeriodicTimer() {}
  void Stop(){};

 private:
  std::function<void()> continuation_;
};

class MockTimerApiManagerEnvironment : public MockApiManagerEnvironmentWithLog {
 public:
  void Log(LogLevel level, const char *message) {
    std::stringstream ss;

    switch (level) {
      case DEBUG:
        ss << "DEBUG ";
        break;
      case INFO:
        ss << "INFO ";
        break;
      case WARNING:
        ss << "WARNING ";
        break;
      case ERROR:
        ss << "ERROR ";
        break;
    }
    ss << std::string(message);

    log_messages_.push_back(ss.str());
  }

  MOCK_METHOD1(MakeTag, void *(std::function<void(bool)>));

  virtual std::unique_ptr<PeriodicTimer> StartPeriodicTimer(
      std::chrono::milliseconds interval, std::function<void()> continuation) {
    return std::unique_ptr<PeriodicTimer>(new MockPeriodicTimer(continuation));
  }

  MOCK_METHOD1(DoRunHTTPRequest, void(HTTPRequest *));
  MOCK_METHOD1(DoRunGRPCRequest, void(GRPCRequest *));
  virtual void RunHTTPRequest(std::unique_ptr<HTTPRequest> req) {
    DoRunHTTPRequest(req.get());
  }
  virtual void RunGRPCRequest(std::unique_ptr<GRPCRequest> req) {
    DoRunGRPCRequest(req.get());
  }

  const std::vector<std::string> &getLogMessage() { return log_messages_; }

 private:
  std::unique_ptr<PeriodicTimer> periodic_timer_;
  std::vector<std::string> log_messages_;
};

class RewriteRuleTest : public ::testing::Test {};

TEST_F(RewriteRuleTest, MatchAndReplacementPattern) {
  MockTimerApiManagerEnvironment env;

  struct testData {
    std::string pattern;
    std::string replacement;
    std::string uri;
    bool matched;
    std::string destination;
  } test_cases[] = {{
                        "^/apis/shelves\\?id=(.*)&key=(.*)$",         //
                        "/shelves/$1?key=$2",                         //
                        "/apis/shelves?id=1&key=this-is-an-api-key",  //
                        true,                                         //
                        "/shelves/1?key=this-is-an-api-key"           //
                    },
                    {
                        "^/api/(.*)$",                          //
                        "/$1",                                  //
                        "/api/shelves?key=this-is-an-api-key",  //
                        true,                                   //
                        "/shelves?key=this-is-an-api-key"       //
                    },
                    {
                        "^/api/(.*)$",                          //
                        "/$$1",                                 //
                        "/api/shelves?key=this-is-an-api-key",  //
                        true,                                   //
                        "/$shelves?key=this-is-an-api-key"      //
                    },
                    {
                        "^/api/v(1|2)/([^/]+)/([^.]+.+)",  //
                        "/api/$2/v$1/$3",                  //
                        "/api/v1/service/list",            //
                        true,                              //
                        "/api/service/v1/list"             //
                    },
                    {
                        "^/api/(.*)$",                              //
                        "/$1",                                      //
                        "/foo/api/shelves?key=this-is-an-api-key",  //
                        false,                                      //
                        ""                                          //
                    }};

  for (auto tc : test_cases) {
    RewriteRule rr(tc.pattern, tc.replacement, &env);

    std::string destination;
    EXPECT_EQ(rr.Check(tc.uri, &destination, false), tc.matched);
    EXPECT_EQ(tc.destination, destination);
  }
}

TEST_F(RewriteRuleTest, InvalidRegexPattern) {
  MockTimerApiManagerEnvironment env;

  RewriteRule rr("/api/\(.\\*\\)", "/$1", &env);

  std::string destination;
  EXPECT_FALSE(
      rr.Check("/api/shelves?key=AIzaSyCfvOENA9MbRupfKQau2X_l8NGMVWF_byI",
               &destination, true));
  EXPECT_FALSE(rr.Check("/shelves?key=AIzaSyCfvOENA9MbRupfKQau2X_l8NGMVWF_byI",
                        &destination, true));

  EXPECT_EQ(env.getLogMessage().size(), 3);
  EXPECT_EQ(env.getLogMessage()[0], kExpectedRewriteErrorLog);
  EXPECT_EQ(env.getLogMessage()[1], kExpectedNotInitializedLog);
  EXPECT_EQ(env.getLogMessage()[2], kExpectedNotInitializedLog);
}

TEST_F(RewriteRuleTest, CheckWithDebugInformation) {
  MockTimerApiManagerEnvironment env;

  RewriteRule rr("/api/(.*)", "/$1", &env);

  std::string destination;
  EXPECT_TRUE(
      rr.Check("/api/shelves?key=AIzaSyCfvOENA9MbRupfKQau2X_l8NGMVWF_byI",
               &destination, true));
  EXPECT_EQ("/shelves?key=AIzaSyCfvOENA9MbRupfKQau2X_l8NGMVWF_byI",
            destination);
  EXPECT_FALSE(rr.Check("/shelves?key=AIzaSyCfvOENA9MbRupfKQau2X_l8NGMVWF_byI",
                        &destination, true));

  EXPECT_EQ(env.getLogMessage().size(), 2);
  EXPECT_EQ(env.getLogMessage()[0], kExpectedRewriteLog);
}

}  // namespace

}  // namespace api_manager
}  // namespace google
