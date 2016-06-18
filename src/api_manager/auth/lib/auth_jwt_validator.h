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
#ifndef API_MANAGER_AUTH_LIB_AUTH_JWT_VALIDATOR_H_
#define API_MANAGER_AUTH_LIB_AUTH_JWT_VALIDATOR_H_

#include <chrono>
#include <cstdlib>
#include <memory>

#include "include/api_manager/auth.h"

namespace google {
namespace api_manager {
namespace auth {

class JwtValidator {
 public:
  // Create JwtValidator with JWT.
  static std::unique_ptr<JwtValidator> Create(const char *jwt, size_t jwt_len);

  // Parse JWT.
  // Returns true when parsing is successful, and fills user_info.
  // Returns false otherwise, and fills error.
  // TODO: migrate to use Status.
  virtual bool Parse(UserInfo *user_info, const char **error) = 0;

  // Verify signature.
  // Returns true when signature verification is successful.
  // Returns false otherwise, and fills error.
  // TODO: migrate to use Status.
  virtual bool VerifySignature(const char *pkey, size_t pkey_len,
                               const char **error) = 0;

  // Returns the expiration time of the JWT.
  virtual std::chrono::system_clock::time_point &GetExpirationTime() = 0;

  virtual ~JwtValidator() {}
};

}  // namespace auth
}  // namespace api_manager
}  // namespace google

#endif /* API_MANAGER_AUTH_LIB_AUTH_JWT_VALIDATOR_H_ */
