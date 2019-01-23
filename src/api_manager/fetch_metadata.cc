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

#include "src/api_manager/fetch_metadata.h"

#include "include/api_manager/http_request.h"
#include "src/api_manager/auth/lib/auth_token.h"

using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace api_manager {

namespace {

// URL path for fetching service-account
const char kMetadataServiceAccountToken[] =
    "/computeMetadata/v1/instance/service-accounts/default/token";

// URL path for fetch instance identity token
const char kMetadataInstanceIdentityToken[] =
    "/computeMetadata/v1/instance/service-accounts/default/identity?audience=";

// The maximum lifetime of a cache token. Unit: seconds.
// Token expired in 1 hour, reduce 100 seconds for grace buffer.
const int kInstanceIdentityTokenExpiration = 3500;

// Initial metadata fetch timeout (1s)
const int kMetadataFetchTimeout = 1000;
// Maximum number of retries to fetch token from metadata
const int kMetadataTokenFetchRetries = 5;
// External status message for failure to fetch service account token
const char kFailedTokenFetch[] = "Failed to fetch service account token";
// External status message for token fetch in progress
const char kFetchingToken[] = "Fetching service account token";
// External status message for token parse failure
const char kFailedTokenParse[] = "Failed to parse access token response";
// Time window (in seconds) before expiration to initiate re-fetch
const char kTokenRefetchWindow = 60;

// Issues a HTTP request to fetch the metadata.
void FetchMetadata(
    context::GlobalContext *context, const std::string &path, const int retry,
    std::function<void(Status, std::map<std::string, std::string> &&,
                       std::string &&)>
        continuation) {
  std::unique_ptr<HTTPRequest> request(new HTTPRequest(continuation));
  request->set_method("GET")
      .set_url(context->metadata_server() + path)
      .set_header("Metadata-Flavor", "Google")
      .set_timeout_ms(kMetadataFetchTimeout)
      .set_max_retries(retry);
  context->env()->RunHTTPRequest(std::move(request));
}
}  // namespace

void GlobalFetchServiceAccountToken(
    std::shared_ptr<context::GlobalContext> context,
    const std::string &audience,
    std::function<void(Status status)> continuation) {
  const auto env = context->env();

  auth::ServiceAccountToken *token;
  std::string path;
  if (audience.empty()) {
    token = context->service_account_token();
    path = kMetadataServiceAccountToken;
  } else {
    path = kMetadataInstanceIdentityToken + audience;
    token = context->GetInstanceIdentityToken(audience);
  }

  // If metadata server is not configured, skip it
  // If client auth secret is available, skip fetching
  if (context->metadata_server().empty() || token->has_client_secret()) {
    continuation(Status::OK);
    return;
  }

  switch (token->state()) {
    case auth::ServiceAccountToken::FETCHED:
      // If token is going to last longer than the window, continue
      if (token->is_access_token_valid(kTokenRefetchWindow)) {
        continuation(Status::OK);
        return;
      }

      // If token is about to expire, initiate fetching a fresh token
      // Expects token to last significantly longer than time lookahead
      token->set_state(auth::ServiceAccountToken::NONE);

      // The first request within the token re-fetch window will carry on
      // the token fetch, while subsequent requests in the window reuse
      // the old token.
      break;
    case auth::ServiceAccountToken::FETCHING:
      env->LogDebug("Service account token fetch in progress");
      // If token is still valid, continue
      if (token->is_access_token_valid(0)) {
        continuation(Status::OK);
        return;
      }
      break;
    case auth::ServiceAccountToken::FAILED:
      // permanent failure
      continuation(Status(Code::INTERNAL, kFailedTokenFetch));
      return;
    case auth::ServiceAccountToken::NONE:
    default:
      env->LogDebug("Need to fetch service account token");
  }

  token->set_state(auth::ServiceAccountToken::FETCHING);
  FetchMetadata(context.get(), path, kMetadataTokenFetchRetries,
                [env, token, continuation, audience](
                    Status status, std::map<std::string, std::string> &&,
                    std::string &&body) {
                  // fetch failed
                  if (!status.ok()) {
                    env->LogDebug("Failed to fetch service account token");
                    token->set_state(auth::ServiceAccountToken::FAILED);
                    continuation(Status(Code::INTERNAL, kFailedTokenFetch));
                    return;
                  }
                  if (audience.empty()) {
                    if (!token->SetTokenJsonResponse(body)) {
                      env->LogDebug("Failed to parse token response body");
                      continuation(Status(Code::INTERNAL, kFailedTokenParse));
                      return;
                    }
                  } else {
                    // TODO: parse JWT to get expiration time.
                    token->set_access_token(body,
                                            kInstanceIdentityTokenExpiration);
                  }
                  continuation(Status::OK);
                });
}

// Fetchs service account token from metadata server.
void FetchServiceAccountToken(
    std::shared_ptr<context::RequestContext> request_context,
    std::function<void(utils::Status)> on_done) {
  std::string audience;
  GlobalFetchServiceAccountToken(
      request_context->service_context()->global_context(), audience, on_done);
}

// Fetches instance identity token from metadata server.
void FetchInstanceIdentityToken(
    std::shared_ptr<context::RequestContext> request_context,
    std::function<void(utils::Status)> on_done) {
  if (!request_context->method()) {
    on_done(Status::OK);
    return;
  }

  const auto &audience = request_context->method()->backend_jwt_audience();
  if (audience.empty()) {
    on_done(Status::OK);
    return;
  }
  GlobalFetchServiceAccountToken(
      request_context->service_context()->global_context(), audience, on_done);
}

}  // namespace api_manager
}  // namespace google
