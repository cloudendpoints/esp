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
#include "src/nginx/grpc.h"

#include "src/api_manager/grpc/proxy_flow.h"
#include "src/nginx/environment.h"
#include "src/nginx/error.h"
#include "src/nginx/grpc_passthrough_server_call.h"
#include "src/nginx/module.h"
#include "src/nginx/transcoded_grpc_server_call.h"
#include "src/nginx/util.h"

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {
namespace nginx {

namespace {

const ngx_str_t kContentTypeApplicationGrpc = ngx_string("application/grpc");
const ngx_str_t kContentTypeApplicationGrpcProto =
    ngx_string("application/grpc+proto");

// The prefix used for GRPC passthrough synthetic locations.
//
// The grpc_pass directive can natively handle GRPC traffic, but needs
// to use another bit of code to handle non-GRPC traffic.  Since that
// code needs to be a request handler, and there can be only one
// request handler for a location, the grpc_pass directive creates a
// synthetic location with this prefix (with an integer appended to
// support multiple grpc_pass directives in a configuration), and
// configures that location according to the configuration block
// supplied to grpc_pass.
//
// Non-GRPC requests are then passed to that synthetic location for
// processing, and the config author may decide what happens at that
// point.  Typically, the Nginx HTTP proxy module is used as the
// handler, via the proxy_pass directive.
const char kGrpcPassLocPrefix[] = "/~~endpoints~grpc_pass~";

// The current upper bound on the number of GRPC passthrough synthetic
// locations.
size_t GrpcPassLocCount = 0;

ngx_int_t GrpcProxyNonGrpcTraffic(ngx_http_request_t *r) {
  ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                "GrpcProxyNonGrpcTraffic: Handling non-GRPC traffic");
  // Proxy the request via grpc_pass's block's configuration, by
  // prepending the grpc_passthrough_prefix for this HTTP location.
  ngx_esp_loc_conf_t *espcf = reinterpret_cast<ngx_esp_loc_conf_t *>(
      ngx_http_get_module_loc_conf(r, ngx_esp_module));

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "redirecting non-GRPC traffic via \"%V\"",
                 &espcf->grpc_passthrough_prefix);

  // Compose the redirect URI, which is espcf->grpc_passthrough_prefix
  // concatenated with r->uri.
  //
  // Note that the prefix ends with a '/' and the request URI begins
  // with a '/'.  We eliminate the duplicate by omiting the first
  // character of the request URI; the final URI length is one less
  // than the two substring lengths put together (although we then
  // allocate one extra byte, to leave space for a trailing NUL).
  ngx_str_t uri;
  uri.len = espcf->grpc_passthrough_prefix.len + r->uri.len - 1;
  uri.data = new (r->pool) u_char[uri.len + 1];
  if (!uri.data) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }
  u_char *pnul =
      ngx_cpystrn(uri.data, espcf->grpc_passthrough_prefix.data, uri.len + 1);
  ngx_cpystrn(pnul, r->uri.data + 1, uri.len - (pnul - uri.data) + 1);

  // This code is a little tricky.
  //
  // The problem here is ngx_http_internal_redirect is about to clear
  // the ESP module's per-request context from the request, and then
  // process the request as though it were a new request.  By the time
  // ngx_http_internal_redirect returns, it may have invoked the ESP
  // module, causing ESP to redo all of its checks.
  //
  // So we need to detect, the second time through ESP, that we've
  // already done ESP's checks, determined the method being called,
  // &c.  But there's no good way to smuggle the endpoints context for
  // the current location into the context for the redirected location
  // -- by the time ngx_http_internal_redirect returns, it's too late.

  // So instead we smuggle it in a relatively hacky way, taking
  // advantage of Nginx's non-blocking single-threaded architecture:
  // we set a global to point to the current request context.  We'll
  // use this when we create a request context, copying over the bits
  // of ESP data needed for the response filter path.
  ngx_esp_current_request_context = ngx_http_esp_ensure_module_ctx(r);

  // Do the redirect.  Nginx's internal redirection machinery takes
  // over the request at this point.  Note that the request contexts
  // will always be reset before this call returns.
  ngx_int_t rc = ngx_http_internal_redirect(r, &uri, &r->args);

  // Just in case the endpoints request context wasn't accessed, make
  // sure it's accessed, so that the variables are always copied
  // before we return.
  ngx_http_esp_ensure_module_ctx(r);

  // And now undo the smuggling.
  ngx_esp_current_request_context = nullptr;

  return rc;
}

std::pair<Status, std::string> GrpcGetBackendAddress(
    ngx_log_t *log, ngx_esp_loc_conf_t *espcf, ngx_esp_request_ctx_t *ctx) {
  if (espcf->grpc_backend_address_override.data &&
      espcf->grpc_backend_address_override.len > 0) {
    ngx_log_error(NGX_LOG_DEBUG, log, 0,
                  "GrpcGetBackendAddress: using override address \"%V\"",
                  &espcf->grpc_backend_address_override);
    return std::make_pair(Status::OK,
                          ngx_str_to_std(espcf->grpc_backend_address_override));
  }
  std::string backend_address = ctx->request_handler->GetBackendAddress();
  if (!backend_address.empty()) {
    ngx_log_error(NGX_LOG_DEBUG, log, 0,
                  "GrpcGetBackendAddress: using config address \"%s\"",
                  backend_address.c_str());
    return std::make_pair(Status::OK, backend_address);
  }
  if (espcf->grpc_backend_address_fallback.data &&
      espcf->grpc_backend_address_fallback.len > 0) {
    ngx_log_error(NGX_LOG_DEBUG, log, 0,
                  "GrpcGetBackendAddress: using fallback address \"%V\"",
                  &espcf->grpc_backend_address_fallback);
    return std::make_pair(Status::OK,
                          ngx_str_to_std(espcf->grpc_backend_address_fallback));
  }
  return std::make_pair(
      Status(NGX_DECLINED, "No GRPC backend address specified"), std::string());
}

std::pair<Status, std::shared_ptr<::grpc::GenericStub>> GrpcGetStub(
    ngx_http_request_t *r, ngx_esp_loc_conf_t *espcf,
    ngx_esp_request_ctx_t *ctx) {
  Status status = Status::OK;
  std::string address;
  std::tie(status, address) =
      GrpcGetBackendAddress(r->connection->log, espcf, ctx);
  if (!status.ok()) {
    return std::make_pair(status, std::shared_ptr<::grpc::GenericStub>());
  }
  ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                "GrpcGetStub: connecting to backend=%s", address.c_str());

  auto it = espcf->grpc_stubs.find(address);
  if (it != espcf->grpc_stubs.end()) {
    return std::make_pair(Status::OK, it->second);
  }

  auto result = std::make_shared<::grpc::GenericStub>(
      ::grpc::CreateChannel(address, ::grpc::InsecureChannelCredentials()));

  if (result) {
    espcf->grpc_stubs.emplace(address, result);
    return std::make_pair(Status::OK, result);
  }

  return std::make_pair(Status(NGX_HTTP_INTERNAL_SERVER_ERROR,
                               "Unable to create channel to GRPC backend"),
                        std::shared_ptr<::grpc::GenericStub>());
}

// The content handler for locations configured with grpc_pass.
ngx_int_t GrpcBackendHandler(ngx_http_request_t *r) {
  ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                "GrpcBackendHandler: Handling request");

  ngx_esp_loc_conf_t *espcf = reinterpret_cast<ngx_esp_loc_conf_t *>(
      ngx_http_get_module_loc_conf(r, ngx_esp_module));
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r);

  if (IsGrpcRequest(r)) {
    // Check whether there's a GRPC backend defined for this request;
    // if there is, we can use it for GRPC traffic.  Otherwise, we'll
    // take the next-best option: falling through and passing the
    // request to the handler defined by the grpc_pass block's
    // synthetic location.

    Status status = Status::OK;
    std::shared_ptr<::grpc::GenericStub> stub;
    std::tie(status, stub) = GrpcGetStub(r, espcf, ctx);

    if (status.ok()) {
      // We have a stub for this backend; proxy the call via libgrpc.

      // TODO: Fill in the headers from the request.
      std::multimap<std::string, std::string> headers;
      auto server_call = std::make_shared<NgxEspGrpcPassThroughServerCall>(r);
      std::string method(reinterpret_cast<char *>(r->uri.data), r->uri.len);

      ngx_log_debug1(NGX_LOG_DEBUG, r->connection->log, 0,
                     "GrpcBackendHandler: gRPC pass-through - method %s",
                     method.c_str());

      grpc::ProxyFlow::Start(
          espcf->esp_srv_conf->esp_main_conf->grpc_queue.get(),
          std::move(server_call), std::move(stub), method, headers);
      return NGX_DONE;
    }

    if (status.code() != NGX_DECLINED) {
      return ngx_esp_return_grpc_error(r, status);
    }

    // Otherwise, fall through to the non-GRPC path.
  } else if (ctx && ctx->request_handler &&
             ctx->request_handler->CanBeTranscoded()) {
    // Same as the gRPC case. Check whether there's a GRPC backend defined for
    // this request to use.
    Status status = Status::OK;
    std::shared_ptr<::grpc::GenericStub> stub;
    std::tie(status, stub) = GrpcGetStub(r, espcf, ctx);

    if (status.ok()) {
      std::shared_ptr<NgxEspTranscodedGrpcServerCall> server_call;
      status = NgxEspTranscodedGrpcServerCall::Create(r, &server_call);
      if (status.ok()) {
        auto method = ctx->request_handler->GetRpcMethodFullName();

        ngx_log_debug1(NGX_LOG_DEBUG, r->connection->log, 0,
                       "GrpcBackendHandler: transcoding - method %s",
                       method.c_str());

        // TODO: Fill in the headers from the request.
        std::multimap<std::string, std::string> headers;
        grpc::ProxyFlow::Start(
            espcf->esp_srv_conf->esp_main_conf->grpc_queue.get(),
            std::move(server_call), std::move(stub), method, headers);
        return NGX_DONE;
      }
    }

    if (status.code() != NGX_DECLINED) {
      ctx->status = status;
      return ngx_esp_return_json_error(r);
    }

    // Otherwise, fall through to the non-GRPC path.
  }

  return GrpcProxyNonGrpcTraffic(r);
}

// Creates an internal-only nginx HTTP location for proxying non-GRPC
// traffic.
char *CreateLocation(ngx_conf_t *cf, ngx_http_conf_ctx_t **ctx_out,
                     ngx_str_t *name_out) {
  // Allocate a zeroed ngx_http_conf_ctx_t.
  ngx_http_conf_ctx_t *ctx = new (cf->pool) ngx_http_conf_ctx_t;
  if (ctx == nullptr) {
    return reinterpret_cast<char *>(NGX_CONF_ERROR);
  }

  // Set the main and http server configuration for the new context.
  ngx_http_conf_ctx_t *pctx = reinterpret_cast<ngx_http_conf_ctx_t *>(cf->ctx);
  ctx->main_conf = pctx->main_conf;
  ctx->srv_conf = pctx->srv_conf;

  // Set up the per-module location configuration array for the new context.
  ctx->loc_conf = new (cf->pool) void *[ngx_http_max_module];
  if (ctx->loc_conf == nullptr) {
    return reinterpret_cast<char *>(NGX_CONF_ERROR);
  }

  // Give every HTTP module a chance to create its per-location
  // configuration structure.
  for (ngx_uint_t i = 0; ngx_modules[i]; i++) {
    if (ngx_modules[i]->type != NGX_HTTP_MODULE) {
      continue;
    }

    ngx_http_module_t *module =
        reinterpret_cast<ngx_http_module_t *>(ngx_modules[i]->ctx);

    if (module->create_loc_conf) {
      void *mconf = module->create_loc_conf(cf);
      if (!mconf) {
        return reinterpret_cast<char *>(NGX_CONF_ERROR);
      }
      ctx->loc_conf[ngx_modules[i]->ctx_index] = mconf;
    }
  }

  // Build a name for the new location, allocating enough space for
  // the prefix, any size_t, and a trailing '/'.
  //
  // Note that this could be one byte less, since ngx_snprintf doesn't
  // add a trailing NUL and ngx_string carries a length.  For sanity,
  // we allocate space for the trailing NUL anyway.
  name_out->len = sizeof(kGrpcPassLocPrefix) + NGX_SIZE_T_LEN + 1;
  name_out->data = new (cf->pool) u_char[name_out->len];

  if (name_out->data == nullptr) {
    return reinterpret_cast<char *>(NGX_CONF_ERROR);
  }

  name_out->len = (ngx_snprintf(name_out->data, name_out->len - 1, "%s%z/",
                                kGrpcPassLocPrefix, ++GrpcPassLocCount) -
                   name_out->data);
  name_out->data[name_out->len] = '\0';

  // Fill in the HTTP core module config.
  ngx_http_core_loc_conf_t *clcf = reinterpret_cast<ngx_http_core_loc_conf_t *>(
      ngx_http_get_module_loc_conf(ctx, ngx_http_core_module));

  clcf->loc_conf = ctx->loc_conf;
  clcf->name = *name_out;

  // Mark this location as internal.  With this set, unless a request
  // has the internal flag set, ngx_http_core_find_config_phase will
  // return NGX_HTTP_NOT_FOUND.  The net effect is to hide external
  // accesses to this location, while keeping it usable for redirected
  // requests.
  clcf->internal = 1;

  // Don't use regex matching for this location.
  clcf->noregex = 1;

  // Link the new location into the current HTTP server.
  ngx_http_core_loc_conf_t *pclcf =
      reinterpret_cast<ngx_http_core_loc_conf_t *>(
          pctx->loc_conf[ngx_http_core_module.ctx_index]);

  if (ngx_http_add_location(cf, &pclcf->locations, clcf) != NGX_OK) {
    return reinterpret_cast<char *>(NGX_CONF_ERROR);
  }

  *ctx_out = ctx;

  return NGX_CONF_OK;
}

}  // namespace

bool IsGrpcRequest(ngx_http_request_t *r) {
  ngx_table_elt_t *ct = r->headers_in.content_type;
  return (ct &&
          (((ct->value.len == kContentTypeApplicationGrpc.len) &&
            !ngx_strncmp(ct->value.data, kContentTypeApplicationGrpc.data,
                         ct->value.len)) ||
           ((ct->value.len == kContentTypeApplicationGrpcProto.len) &&
            !ngx_strncmp(ct->value.data, kContentTypeApplicationGrpcProto.data,
                         ct->value.len))));
}

char *ConfigureGrpcBackendHandler(ngx_conf_t *cf, ngx_command_t *cmd,
                                  void *conf) {
  ngx_esp_loc_conf_t *espcf = reinterpret_cast<ngx_esp_loc_conf_t *>(conf);
  if (espcf->grpc_pass_block) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "duplicate grpc_pass directive");
    return reinterpret_cast<char *>(NGX_CONF_ERROR);
  }
  espcf->grpc_pass_block = 1;

  // Set up a synthetic location for processing non-API traffic.
  ngx_http_conf_ctx_t *ctx;
  char *rv = CreateLocation(cf, &ctx, &espcf->grpc_passthrough_prefix);
  if (rv != NGX_CONF_OK) {
    return rv;
  }

  // Set ourselves up as the handler for the current location.
  ngx_http_core_loc_conf_t *clcf = reinterpret_cast<ngx_http_core_loc_conf_t *>(
      ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module));
  clcf->handler = GrpcBackendHandler;

  // Process the grpc_pass trailing block, using it to configure the
  // synthetic location.
  ngx_conf_t save;
  save = *cf;
  cf->ctx = ctx;
  cf->cmd_type = NGX_HTTP_LOC_CONF;
  rv = ngx_conf_parse(cf, nullptr);
  *cf = save;

  return rv;
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
