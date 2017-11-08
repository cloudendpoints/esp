// Copyright (C) Extensible Service Proxy Authors
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

#include "src/grpc/proxy_flow.h"
#include "src/nginx/environment.h"
#include "src/nginx/error.h"
#include "src/nginx/grpc_passthrough_server_call.h"
#include "src/nginx/grpc_web_server_call.h"
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

  ::grpc::ChannelArguments channel_arguments;

  channel_arguments.SetMaxReceiveMessageSize(INT_MAX);
  channel_arguments.SetMaxSendMessageSize(INT_MAX);

  auto result =
      std::make_shared<::grpc::GenericStub>(::grpc::CreateCustomChannel(
          address, ::grpc::InsecureChannelCredentials(), channel_arguments));

  if (result) {
    espcf->grpc_stubs.emplace(address, result);
    return std::make_pair(Status::OK, result);
  }

  return std::make_pair(Status(NGX_HTTP_INTERNAL_SERVER_ERROR,
                               "Unable to create channel to GRPC backend"),
                        std::shared_ptr<::grpc::GenericStub>());
}

std::multimap<std::string, std::string> ExtractMetadata(ngx_http_request_t *r) {
  std::multimap<std::string, std::string> metadata;

  for (auto &h : r->headers_in) {
    metadata.emplace(ngx_str_to_std({h.key.len, h.lowcase_key}),
                     ngx_str_to_std(h.value));
  }

  return metadata;
}

bool IsGrpcWeb(ngx_http_request_t *r) {
  if (r != nullptr && r->headers_in.content_type) {
    ::google::protobuf::StringPiece content_type =
        ngx_str_to_stringpiece(r->headers_in.content_type->value);
    if (r->method == NGX_HTTP_POST &&
        (content_type == "application/grpc-web" ||
         content_type == "application/grpc-web+proto")) {
      return true;
    }
  }
  return false;
}

bool CanBeTranscoded(ngx_esp_request_ctx_t *ctx) {
  // Verify that all the necessary pieces exist and the method has RPC info
  // configured
  return ctx->transcoder_factory && ctx->request_handler->method() &&
         !ctx->request_handler->method()->rpc_method_full_name().empty() &&
         !ctx->request_handler->method()->request_type_url().empty() &&
         !ctx->request_handler->method()->response_type_url().empty();
}

// The content handler for locations configured with grpc_pass.
ngx_int_t GrpcBackendHandler(ngx_http_request_t *r) {
  ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                "GrpcBackendHandler: Handling request");

  ngx_esp_loc_conf_t *espcf = reinterpret_cast<ngx_esp_loc_conf_t *>(
      ngx_http_get_module_loc_conf(r, ngx_esp_module));
  ngx_esp_main_conf_t *espmf = reinterpret_cast<ngx_esp_main_conf_t *>(
      ngx_http_get_module_main_conf(r, ngx_esp_module));
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r);

  Status status = Status::OK;
  if (ctx->grpc_pass_through) {
    // Check whether there's a GRPC backend defined for this request;
    // if there is, we can use it for GRPC traffic.  Otherwise, we'll
    // take the next-best option: falling through and passing the
    // request to the handler defined by the grpc_pass block's
    // synthetic location.

    ctx->grpc_backend = true;
    std::shared_ptr<::grpc::GenericStub> stub;
    std::tie(status, stub) = GrpcGetStub(r, espcf, ctx);

    if (status.ok()) {
      // We have a stub for this backend; proxy the call via libgrpc.
      const std::multimap<std::string, std::string> &headers =
          ExtractMetadata(r);
      std::shared_ptr<NgxEspGrpcPassThroughServerCall> server_call;
      status = NgxEspGrpcPassThroughServerCall::Create(r, &server_call);

      if (status.ok()) {
        std::string method(reinterpret_cast<char *>(r->uri.data), r->uri.len);

        ngx_log_debug1(NGX_LOG_DEBUG, r->connection->log, 0,
                       "GrpcBackendHandler: gRPC pass-through - method %s",
                       method.c_str());

        grpc::ProxyFlow::Start(espmf->grpc_queue.get(), std::move(server_call),
                               std::move(stub), method, headers);
        return NGX_DONE;
      }
    }
  } else if (ctx && ctx->request_handler && IsGrpcWeb(r)) {
    ctx->grpc_backend = true;
    std::shared_ptr<::grpc::GenericStub> stub;
    std::tie(status, stub) = GrpcGetStub(r, espcf, ctx);

    if (status.ok()) {
      // We have a stub for this backend; proxy the call via libgrpc.
      const std::multimap<std::string, std::string> &headers =
          ExtractMetadata(r);
      std::shared_ptr<NgxEspGrpcWebServerCall> server_call;
      status = NgxEspGrpcWebServerCall::Create(r, &server_call);

      if (status.ok()) {
        std::string method(reinterpret_cast<char *>(r->uri.data), r->uri.len);

        ngx_log_debug1(NGX_LOG_DEBUG, r->connection->log, 0,
                       "GrpcBackendHandler: gRPC-Web - method %s",
                       method.c_str());

        grpc::ProxyFlow::Start(espmf->grpc_queue.get(), std::move(server_call),
                               std::move(stub), method, headers);
        return NGX_DONE;
      }
    }
  } else if (ctx && ctx->request_handler && CanBeTranscoded(ctx)) {
    // Same as the gRPC case. Check whether there's a GRPC backend defined for
    // this request to use.
    ctx->grpc_backend = true;
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

        const std::multimap<std::string, std::string> &headers =
            ExtractMetadata(r);
        grpc::ProxyFlow::Start(espmf->grpc_queue.get(), std::move(server_call),
                               std::move(stub), method, headers);
        return NGX_DONE;
      }
    }
  } else {
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                  "GrpcRejectNonGrpcTraffic: Rejecting non-GRPC traffic");

    // Returning 404 - NOT FOUND
    status = Status(NGX_HTTP_NOT_FOUND,
                    "No HTTP backend defined for this location.");
  }

  ctx->status = status;
  return ngx_esp_return_error(r);
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
  if (espcf->grpc_pass) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "duplicate grpc_pass directive");
    return reinterpret_cast<char *>(NGX_CONF_ERROR);
  }
  espcf->grpc_pass = 1;

  // Set ourselves up as the handler for the current location.
  ngx_http_core_loc_conf_t *clcf = reinterpret_cast<ngx_http_core_loc_conf_t *>(
      ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module));
  clcf->handler = GrpcBackendHandler;

  if (cf->args->nelts > 1) {
    ngx_str_t *argv = reinterpret_cast<ngx_str_t *>(cf->args->elts);
    // if "override" is specified, argv[1] is the override address; otherwise
    // it's the fallback address.
    if (cf->args->nelts == 3) {
      if (!ngx_string_equal(argv[2], ngx_string("override"))) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "Invalid second parameter for grpc_pass: '%V'. "
                           "Expected 'override'.",
                           &argv[2]);
        return reinterpret_cast<char *>(NGX_CONF_ERROR);
      }
      espcf->grpc_backend_address_override = argv[1];
      ngx_log_debug1(NGX_LOG_DEBUG, cf->log, 0,
                     "ConfigureGrpcBackendHandler: override address %V",
                     espcf->grpc_backend_address_override);
    } else {
      espcf->grpc_backend_address_fallback = argv[1];
      ngx_log_debug1(NGX_LOG_DEBUG, cf->log, 0,
                     "ConfigureGrpcBackendHandler: fallback address %V",
                     espcf->grpc_backend_address_fallback);
    }
  }

  return NGX_CONF_OK;
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
