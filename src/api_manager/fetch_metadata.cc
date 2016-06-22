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

#include "src/api_manager/fetch_metadata.h"

#include <sstream>

#include "include/api_manager/http_request.h"
#include "src/api_manager/auth/lib/auth_token.h"

using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace api_manager {

namespace {

// URL path for fetching compute metadata.
const char kComputeMetadata[] = "/computeMetadata/v1/?recursive=true";
// URL path for fetching service-account
const char kMetadataServiceAccountToken[] =
    "/computeMetadata/v1/instance/service-accounts/default/token";

// External status for failed metadata fetch
const Status failed_metadata_fetch =
    Status(Code::UNAVAILABLE, "Failed to fetch metadata", Status::INTERNAL);

// External status for failed service account token fetch
const Status failed_token_fetch =
    Status(Code::UNAVAILABLE, "Failed to fetch service account token",
           Status::INTERNAL);

// Issues a HTTP request to fetch the metadata.
Status FetchMetadata(context::RequestContext *context, const char *path,
                     std::function<void(Status, std::string &&)> continuation) {
  std::unique_ptr<HTTPRequest> request(new HTTPRequest(continuation));
  request->set_method("GET")
      .set_url(context->service_context()->metadata_server() + path)
      .set_header("Metadata-Flavor", "Google")
      .set_timeout_ms(1000);  // 1s timeout to fetch metadata.
  return context->service_context()->env()->RunHTTPRequest(std::move(request));
}
}  // namespace

void FetchGceMetadata(std::shared_ptr<context::RequestContext> context,
                      std::function<void(Status)> continuation) {
  if (context->service_context()->metadata_server().empty()) {
    // No need to fetching metadata, metadata server address is not set.
    continuation(Status::OK);
    return;
  }

  auto env = context->service_context()->env();
  switch (context->service_context()->gce_metadata()->state()) {
    case GceMetadata::DONE_STATE:
      // Already have metadata.
      env->LogDebug("Metadata already available. Fetch skipped.");
      continuation(Status::OK);
      return;
    case GceMetadata::FAILED_STATE:
      // Metadata fetch already failed.
      // TODO: retry after some timeout?
      // TODO: at some point, timeout of metadata fetch will become an
      // indication of not running in a GCE VM and will be handled as a success.
      // Log debug only because if this happens, this log will happen on every
      // API call.
      env->LogDebug("Metadata fetch previously failed. Skipping with error.");
      continuation(failed_metadata_fetch);
      return;
    case GceMetadata::FETCHING_STATE:
      // TODO: do not issue another metadata call, wait for the current one
      // to finish. Hang the continuation in a queue.
      env->LogWarning("Another request fetching metadata. Duplicate fetch.");
      break;
    case GceMetadata::INVALID_STATE:
    default:
      env->LogDebug("Fetching metadata.");
      break;
  }

  context->service_context()->gce_metadata()->set_state(
      GceMetadata::FETCHING_STATE);
  Status status = FetchMetadata(
      context.get(), kComputeMetadata,
      [context, continuation](Status status, std::string &&body) {
        if (status.ok()) {
          // reassing to parsing status
          status =
              context->service_context()->gce_metadata()->ParseFromJson(&body);
        } else {
          // http fetch error
          status = failed_metadata_fetch;
        }
        context->service_context()->gce_metadata()->set_state(
            status.ok() ? GceMetadata::DONE_STATE : GceMetadata::FAILED_STATE);
        continuation(status);
      });

  // If failed, continuation will not be called by FetchMetadata().
  if (!status.ok()) {
    context->service_context()->gce_metadata()->set_state(
        GceMetadata::FAILED_STATE);
    continuation(failed_metadata_fetch);
  }
}

void FetchServiceAccountToken(std::shared_ptr<context::RequestContext> context,
                              std::function<void(Status status)> continuation) {
  // If metadata server is not configured, skip it.
  // If service account token is fetched and not expired yet, skip it.
  if (context->service_context()->metadata_server().empty() ||
      !context->service_context()
           ->service_account_token()
           ->NeedToFetchAccessToken()) {
    continuation(Status::OK);
    return;
  }

  auto on_done = [context, continuation](Status status, std::string &&body) {
    if (!status.ok()) {
      continuation(failed_token_fetch);
      return;
    }

    char *token = nullptr;
    int expires = 0;
    if (!auth::esp_get_service_account_auth_token(
            const_cast<char *>(body.data()), body.length(), &token, &expires) ||
        token == nullptr) {
      continuation(Status(Code::INVALID_ARGUMENT,
                          "Failed to parse access token response"));
      return;
    }
    // Compute Engine returns tokens with at least 60 seconds life left so we
    // need to wait a bit longer to refresh the token.
    context->service_context()->service_account_token()->SetAccessToken(
        token, expires - 50);
    free(token);

    continuation(Status::OK);
  };

  Status status =
      FetchMetadata(context.get(), kMetadataServiceAccountToken, on_done);
  // If failed, continuation will not be called by FetchMetadata().
  if (!status.ok()) {
    continuation(failed_token_fetch);
  }
}

}  // namespace api_manager
}  // namespace google
