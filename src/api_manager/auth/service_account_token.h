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
#ifndef API_MANAGER_AUTH_SERVICE_ACCOUNT_TOKEN_H_
#define API_MANAGER_AUTH_SERVICE_ACCOUNT_TOKEN_H_

#include <time.h>

#include "include/api_manager/env_interface.h"

namespace google {
namespace api_manager {
namespace auth {

// Stores service account tokens to access Google services, such as service
// control and cloud tracing. There are two kinds of auth token:
// 1) client auth secret is a client secret can be used to generate auth
// JWT token. But JWT token is audience specific. Need to generate auth
// JWT token for each service with its audience.
// 2) GCE service account token is fetched from GCP metadata server.
// This auth token can be used for any Google services.
class ServiceAccountToken {
 public:
  ServiceAccountToken(ApiManagerEnvInterface* env) : env_(env) {}

  // Sets the client auth secret and it can be used to generate JWT token.
  utils::Status SetClientAuthSecret(const std::string& secret);

  // Checks if needs to fetch service account token from metadata server.
  // Returns false if client auth secret exists. Otherwise returns true
  // either auth token doesn't present, or it is expired.
  bool NeedToFetchAccessToken() const;

  // Sets the service account token fetched from the GCP metadata server
  // and its life time.
  void SetAccessToken(const std::string& token, int token_life_in_second);

  // JWT token calcualted from client auth secret are audience dependent.
  enum JWT_TOKEN_TYPE {
    JWT_TOKEN_FOR_SERVICE_CONTROL = 0,
    JWT_TOKEN_FOR_CLOUD_TRACING,
    JWT_TOKEN_TYPE_MAX,
  };
  // Set audience.  Only calcualtes JWT token with specified audience.
  void SetAudience(JWT_TOKEN_TYPE type, const std::string& audience);

  // Gets the auth token to access Google services.
  // If client auth secret is specified, use it to calcualte JWT token.
  // Otherwise, use the access token fetched from metadata server.
  const std::string& GetAuthToken(JWT_TOKEN_TYPE type);

 private:
  // Stores base token info. Used for both OAuth and JWT tokens.
  class TokenInfo {
   public:
    // If token is valid.
    bool is_valid() const;

    void set_token(const std::string& token) { token_ = token; }
    void set_expire_time(time_t expire_time) { expire_time_ = expire_time; }

    // get the token
    const std::string& token() const { return token_; }

   private:
    // The auth token.
    std::string token_;
    // The token expiration time.
    time_t expire_time_;
  };

  // Stores JWT token info
  class JwtTokenInfo : public TokenInfo {
   public:
    void set_audience(const std::string audience) { audience_ = audience; }
    const std::string& audience() const { return audience_; }

    // Generates auth JWT token from client auth secret.
    utils::Status GenerateJwtToken(const std::string& client_auth_secret);

   private:
    // The audiences.
    std::string audience_;
  };

  // environment interface.
  ApiManagerEnvInterface* env_;

  // The client auth secret which can be used to generate JWT auth token.
  std::string client_auth_secret_;

  // JWT tokens calcualted from client auth secrect.
  JwtTokenInfo jwt_tokens_[JWT_TOKEN_TYPE_MAX];

  // GCE service account access token fetched from GCE metadata server.
  TokenInfo access_token_;
};

}  // namespace auth
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_AUTH_SERVICE_ACCOUNT_TOKEN_H_
