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
#include "src/api_manager/context/service_context.h"

#include "src/api_manager/service_control/aggregated.h"
#include "src/api_manager/transcoding/transcoder_factory.h"

namespace google {
namespace api_manager {
namespace context {

namespace {

const char kCloudTraceUrl[] = "https://cloudtrace.googleapis.com";
}

ServiceContext::ServiceContext(std::unique_ptr<ApiManagerEnvInterface> env,
                               std::unique_ptr<Config> config)
    : env_(std::move(env)),
      config_(std::move(config)),
      service_account_token_(env_.get()),
      service_control_(CreateInterface()),
      cloud_trace_config_(CreateCloudTraceConfig()),
      transcoder_factory_(config_->service()) {}

MethodCallInfo ServiceContext::GetMethodCallInfo(const char *http_method,
                                                 size_t http_method_size,
                                                 const char *url,
                                                 size_t url_size) const {
  if (config_ == nullptr) {
    return MethodCallInfo();
  }
  std::string h(http_method, http_method_size);
  std::string u(url, url_size);
  return config_->GetMethodCallInfo(h, u);
}

const std::string &ServiceContext::project_id() const {
  if (gce_metadata_.has_valid_data() && !gce_metadata_.project_id().empty()) {
    return gce_metadata_.project_id();
  } else {
    return config_->service().producer_project_id();
  }
}

std::unique_ptr<service_control::Interface> ServiceContext::CreateInterface() {
  return std::unique_ptr<service_control::Interface>(
      service_control::Aggregated::Create(config_->service(),
                                          config_->server_config(), env_.get(),
                                          &service_account_token_));
}

std::unique_ptr<cloud_trace::CloudTraceConfig>
ServiceContext::CreateCloudTraceConfig() {
  std::string url;
  if (config_->server_config() &&
      !config_->server_config()
           ->cloud_tracing_config()
           .url_override()
           .empty()) {
    url = config_->server_config()->cloud_tracing_config().url_override();
  } else {
    url = kCloudTraceUrl;
  }
  return std::unique_ptr<cloud_trace::CloudTraceConfig>(
      new cloud_trace::CloudTraceConfig(&service_account_token_, url));
}

}  // namespace context
}  // namespace api_manager
}  // namespace google
