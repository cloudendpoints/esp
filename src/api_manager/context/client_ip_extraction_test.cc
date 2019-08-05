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
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "src/api_manager/context/request_context.h"
#include "src/api_manager/context/service_context.h"
#include "src/api_manager/mock_api_manager_environment.h"
//#include "src/api_manager/mock_request.h"
//#include "src/api_manager/api_manager_impl.h"
#include "include/api_manager/request.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {
namespace context {

namespace {

const char kServiceConfig1[] =
    R"(
{
  "name": "bookstore.test.appspot.com",
  "title": "Bookstore",
  "http": {
    "rules": [
      {
        "selector": "EchoGetMessage",
        "get": "/echo"
      }
    ]
  },
  "usage": {
    "rules": [
      {
        "selector": "EchoGetMessage",
        "allowUnregisteredCalls": true
      }
    ]
  },
  "control": {
    "environment": "servicecontrol.googleapis.com"
  },
  "id": "2017-05-01r0"
}
)";

class MockApiManagerEnvironment : public ApiManagerEnvInterface {
 public:
  virtual ~MockApiManagerEnvironment() {}

  void Log(LogLevel level, const char *message) override {
    // std::cout << __FILE__ << ":" << __LINE__ << " " << message << std::endl;
  }
  std::unique_ptr<PeriodicTimer> StartPeriodicTimer(
      std::chrono::milliseconds interval, std::function<void()> continuation) {
    return std::unique_ptr<PeriodicTimer>(new MockPeriodicTimer(continuation));
  }

  void RunHTTPRequest(std::unique_ptr<HTTPRequest> request) override {}

  virtual void RunGRPCRequest(std::unique_ptr<GRPCRequest> request) override {}
};

class MockRequest : public Request {
 public:
  MockRequest(const std::string &client_ip,
              const std::map<std::string, std::string> &header)
      : client_ip_(client_ip), header_(header) {}

  virtual ~MockRequest() {}

  bool FindHeader(const std::string &name, std::string *header) override {
    auto it = header_.find(name);
    if (it != header_.end()) {
      header->assign(it->second);
      return true;
    }

    return false;
  }

  std::string GetClientIP() { return client_ip_; }

  std::string GetRequestHTTPMethod() override { return "GET"; }

  std::string GetQueryParameters() override { return ""; }

  std::string GetUnparsedRequestPath() override { return "/echo"; }

  bool FindQuery(const std::string &name, std::string *query) override {
    return false;
  }

  MOCK_METHOD2(AddHeaderToBackend,
               utils::Status(const std::string &, const std::string &));
  MOCK_METHOD1(SetAuthToken, void(const std::string &));
  MOCK_METHOD0(GetFrontendProtocol,
               ::google::api_manager::protocol::Protocol());
  MOCK_METHOD0(GetBackendProtocol, ::google::api_manager::protocol::Protocol());
  MOCK_METHOD0(GetRequestPath, std::string());
  MOCK_METHOD0(GetInsecureCallerID, std::string());
  MOCK_METHOD0(GetRequestHeaders, std::multimap<std::string, std::string> *());
  MOCK_METHOD0(GetGrpcRequestBytes, int64_t());
  MOCK_METHOD0(GetGrpcResponseBytes, int64_t());
  MOCK_METHOD0(GetGrpcRequestMessageCounts, int64_t());
  MOCK_METHOD0(GetGrpcResponseMessageCounts, int64_t());

 private:
  const std::string client_ip_;
  const std::map<std::string, std::string> header_;
};
}  // namespace

class ClientIPExtractionTest : public ::testing::Test {
 protected:
  ClientIPExtractionTest() : callback_run_count_(0) {}

  void SetUp() {
    callback_run_count_ = 0;
    call_history_.clear();
  }

 protected:
  std::vector<std::string> call_history_;
  int callback_run_count_;
};

// Extracts client IP address from the request based on the server configuration
std::string ExtractClientIP(std::string serverConfig, std::string remote_ip,
                            std::map<std::string, std::string> headers) {
  std::unique_ptr<ApiManagerEnvInterface> env(new MockApiManagerEnvironment());

  std::shared_ptr<context::GlobalContext> global_context(
      new context::GlobalContext(std::move(env), serverConfig));

  std::unique_ptr<Config> config =
      Config::Create(global_context->env(), std::string(kServiceConfig1));

  std::shared_ptr<context::ServiceContext> service_context(
      new context::ServiceContext(global_context, std::move(config)));

  std::unique_ptr<MockRequest> request(new MockRequest(remote_ip, headers));

  RequestContext context(service_context, std::move(request));

  service_control::CheckRequestInfo info;

  context.FillCheckRequestInfo(&info);

  return info.client_ip;
}

TEST_F(ClientIPExtractionTest, ClientIPAddressNoOverrideTest) {
  const char kServerConfigWithoutClientIPExperiment[] =
      R"(
  {
    "experimental": {
      "disable_log_status": false
    }
  }
  )";

  EXPECT_EQ("4.4.4.4",
            ExtractClientIP(kServerConfigWithoutClientIPExperiment, "4.4.4.4",
                            {{"X-Forwarded-For", "1.1.1.1, 2.2.2.2, 3.3.3.3"},
                             {"apiKey", "test-api-key"}}));
}

TEST_F(ClientIPExtractionTest, ClientIPAddressOverrideTest) {
  const char kServerConfigWithClientIPExperimentSecondFromLast[] =
      R"(
  {
    "client_ip_extraction_config": {
      "client_ip_header": "X-Forwarded-For",
      "client_ip_position": -2
    }
  }
  )";

  EXPECT_EQ("2.2.2.2",
            ExtractClientIP(kServerConfigWithClientIPExperimentSecondFromLast,
                            "4.4.4.4",
                            {{"X-Forwarded-For", "1.1.1.1, 2.2.2.2, 3.3.3.3"},
                             {"apiKey", "test-api-key"}}));
}

TEST_F(ClientIPExtractionTest, ClientIPAddressOverrideLastTest) {
  const char kServerConfigWithClientIPExperimentLast[] =
      R"(
  {
    "client_ip_extraction_config": {
      "client_ip_header": "X-Forwarded-For",
      "client_ip_position": -1
    }
  }
  )";

  EXPECT_EQ("3.3.3.3",
            ExtractClientIP(kServerConfigWithClientIPExperimentLast, "4.4.4.4",
                            {{"X-Forwarded-For", "1.1.1.1, 2.2.2.2, 3.3.3.3"},
                             {"apiKey", "test-api-key"}}));
}

TEST_F(ClientIPExtractionTest, ClientIPAddressOverrideOutOfIndexTest) {
  const char kServerConfigWithClientIPExperimentOutOfIndex[] =
      R"(
  {
    "client_ip_extraction_config": {
      "client_ip_header": "X-Forwarded-For",
      "client_ip_position": -5
    }
  }
  )";

  EXPECT_EQ(
      "4.4.4.4",
      ExtractClientIP(kServerConfigWithClientIPExperimentOutOfIndex, "4.4.4.4",
                      {{"X-Forwarded-For", "1.1.1.1, 2.2.2.2, 3.3.3.3"},
                       {"apiKey", "test-api-key"}}));
}

TEST_F(ClientIPExtractionTest, ClientIPAddressOverrideFirstIndexTest) {
  const char kServerConfigWithClientIPExperimentFirst[] =
      R"(
  {
    "client_ip_extraction_config": {
      "client_ip_header": "X-Forwarded-For",
      "client_ip_position": 0
    }
  }
  )";

  EXPECT_EQ("1.1.1.1",
            ExtractClientIP(kServerConfigWithClientIPExperimentFirst, "4.4.4.4",
                            {{"X-Forwarded-For", "1.1.1.1, 2.2.2.2, 3.3.3.3"},
                             {"apiKey", "test-api-key"}}));
}

TEST_F(ClientIPExtractionTest, ClientIPAddressOverrideSecondIndexTest) {
  const char kServerConfigWithClientIPExperimentSecond[] =
      R"(
  {
    "client_ip_extraction_config": {
      "client_ip_header": "X-Forwarded-For",
      "client_ip_position": 1
    }
  }
  )";

  EXPECT_EQ("2.2.2.2", ExtractClientIP(
                           kServerConfigWithClientIPExperimentSecond, "4.4.4.4",
                           {{"X-Forwarded-For", "1.1.1.1, 2.2.2.2, 3.3.3.3"},
                            {"apiKey", "test-api-key"}}));
}

}  // namespace context
}  // namespace api_manager
}  // namespace google
