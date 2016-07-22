/*
 * Copyright (C) Endpoints Server Proxy Authors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef API_MANAGER_CONTEXT_SERVICE_CONTEXT_H_
#define API_MANAGER_CONTEXT_SERVICE_CONTEXT_H_

#include "include/api_manager/transcoder.h"

#include "src/api_manager/auth/certs.h"
#include "src/api_manager/auth/jwt_cache.h"
#include "src/api_manager/auth/service_account_token.h"
#include "src/api_manager/cloud_trace/cloud_trace.h"
#include "src/api_manager/config.h"
#include "src/api_manager/gce_metadata.h"
#include "src/api_manager/method.h"
#include "src/api_manager/service_control/interface.h"
#include "src/api_manager/transcoding/transcoder_factory.h"

namespace google {
namespace api_manager {

namespace context {

// Shared context across request for every service
// Each RequestContext will hold a refcount to this object.
class ServiceContext {
 public:
  ServiceContext(std::unique_ptr<ApiManagerEnvInterface> env,
                 std::unique_ptr<Config> config);

  bool Enabled() const { return RequireAuth() || service_control_; }

  const std::string &service_name() const { return config_->service_name(); }

  void SetMetadataServer(const std::string &server) {
    metadata_server_ = server;
  }

  auth::ServiceAccountToken *service_account_token() {
    return &service_account_token_;
  }

  ApiManagerEnvInterface *env() { return env_.get(); }
  Config *config() { return config_.get(); }

  MethodCallInfo GetMethodCallInfo(const char *http_method,
                                   size_t http_method_size, const char *url,
                                   size_t url_size) const;

  service_control::Interface *service_control() const {
    return service_control_.get();
  }

  bool RequireAuth() const {
    return !is_auth_force_disabled_ && config_->HasAuth();
  }

  auth::Certs &certs() { return certs_; }
  auth::JwtCache &jwt_cache() { return jwt_cache_; }

  bool GetJwksUri(const std::string &issuer, std::string *url) {
    return config_->GetJwksUri(issuer, url);
  }

  void SetJwksUri(const std::string &issuer, const std::string &jwks_uri,
                  bool openid_valid) {
    config_->SetJwksUri(issuer, jwks_uri, openid_valid);
  }

  const std::string &metadata_server() const { return metadata_server_; }
  GceMetadata *gce_metadata() { return &gce_metadata_; }
  const std::string &project_id() const;
  const cloud_trace::CloudTraceConfig *cloud_trace_config() const {
    return cloud_trace_config_.get();
  }

  transcoding::TranscoderFactory *transcoder_factory() {
    return &transcoder_factory_;
  }

  bool is_cloud_trace_force_disabled() const {
    return is_cloud_tracing_force_disabled_;
  }

 private:
  std::unique_ptr<service_control::Interface> CreateInterface();

  std::unique_ptr<cloud_trace::CloudTraceConfig> CreateCloudTraceConfig();

  std::unique_ptr<ApiManagerEnvInterface> env_;
  std::unique_ptr<Config> config_;

  auth::Certs certs_;
  auth::JwtCache jwt_cache_;

  // service account tokens
  auth::ServiceAccountToken service_account_token_;

  // The service control object.
  std::unique_ptr<service_control::Interface> service_control_;

  // The service control object.
  std::unique_ptr<cloud_trace::CloudTraceConfig> cloud_trace_config_;

  // meta data server.
  std::string metadata_server_;
  // GCE metadata
  GceMetadata gce_metadata_;
  // Transcoder factory
  transcoding::TranscoderFactory transcoder_factory_;

  // Is auth force-disabled
  bool is_auth_force_disabled_;
  // Is cloud-tracing force-disabled
  bool is_cloud_tracing_force_disabled_;
};

}  // namespace context
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_CONTEXT_SERVICE_CONTEXT_H_
