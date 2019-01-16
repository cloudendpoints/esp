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
//
// An Extensible Service Proxy nginx module.
//
#include "src/nginx/module.h"

#include <core/ngx_string.h>
#include <memory>

#include "include/api_manager/utils/version.h"
#include "module.h"
#include "src/nginx/config.h"
#include "src/nginx/environment.h"
#include "src/nginx/error.h"
#include "src/nginx/response.h"
#include "src/nginx/status.h"
#include "src/nginx/util.h"
#include "src/nginx/version.h"

using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace api_manager {
namespace nginx {

namespace {

// Internal debugging header
static ngx_str_t kXEndpointsDebugUrlRewrite =
    ngx_string("x-endpoints-debug-url-rewrite");

// decode encoded uri
void url_decode(const std::string &uri, std::string &decoded) {
  decoded.reserve(uri.length());
  for (std::size_t i = 0; i < uri.length(); i++) {
    if (int(uri[i]) == 37) {
      if (uri.length() > i + 2 && isxdigit(uri[i + 1]) &&
          isxdigit(uri[i + 2])) {
        int ii;
        sscanf(uri.substr(i + 1, 2).c_str(), "%x", &ii);

        char ch = static_cast<char>(ii);
        decoded.append(1, ch);
        i = i + 2;
      } else {
        decoded.append(1, uri[i]);
      }
    } else {
      decoded.append(1, uri[i]);
    }
  }
}

// Internally redirect request based on rewrite rule in server config
void ngx_esp_rewrite_uri(ngx_http_request_t *r, ngx_esp_loc_conf_t *lc) {
  std::string debug_header;

  auto h = ngx_esp_find_headers_in(r, kXEndpointsDebugUrlRewrite.data,
                                   kXEndpointsDebugUrlRewrite.len);
  if (h && h->value.len > 0) {
    debug_header.assign(ngx_str_to_std(h->value));
    std::transform(debug_header.begin(), debug_header.end(),
                   debug_header.begin(), ::tolower);
  }

  bool debug_mode = (debug_header == "true");

  std::string unparsed_uri;
  if (lc->esp->ReWriteURL((const char *)r->unparsed_uri.data,
                          r->unparsed_uri.len, &unparsed_uri, debug_mode)) {
    // update r->unparsed_uri
    ngx_str_copy_from_std(r->pool, unparsed_uri, &r->unparsed_uri);
    ngx_str_copy_from_std(r->pool,
                          ngx_str_to_std(r->method_name) + " " + unparsed_uri +
                              " " + ngx_str_to_std(r->http_protocol),
                          &r->request_line);

    std::size_t found = unparsed_uri.find_first_of('?');
    if (found != std::string::npos) {
      std::string uri;

      url_decode(unparsed_uri.substr(0, unparsed_uri.find_first_of('?')), uri);
      ngx_str_copy_from_std(r->pool, uri, &r->uri);
    }
  }
}

// Override the backend path, in order to support backend routing. For more
// information, please refer to
// https://github.com/googleapis/googleapis/blob/master/google/api/backend.proto#L36
void ngx_esp_override_backend_path(ngx_http_request_t *r,
                                   ngx_esp_request_ctx_t *ctx) {
  if (!ctx->request_handler->ShouldOverrideBackend()) {
    return;
  }

  std::string backend_path = ctx->request_handler->GetBackendPath();
  if (!backend_path.empty()) {
    ngx_log_debug1(
        NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "esp: override backend path to: %s, according to backend rule.",
        backend_path.c_str());

    ngx_str_copy_from_std(r->pool, backend_path, &r->unparsed_uri);
    ngx_str_copy_from_std(r->pool,
                          ngx_str_to_std(r->method_name) + " " + backend_path +
                              " " + ngx_str_to_std(r->http_protocol),
                          &r->request_line);

    std::size_t found = backend_path.find_first_of('?');
    if (found != std::string::npos) {
      std::string uri;
      url_decode(backend_path.substr(0, backend_path.find_first_of('?')), uri);
      ngx_str_copy_from_std(r->pool, uri, &r->uri);
    }
  }
}

}  // namespace

struct wakeup_context_s {
 public:
  wakeup_context_s(ngx_http_request_t *r, ngx_esp_request_ctx_t *ctx)
      : request(r), request_context(ctx) {}

  ngx_http_request_t *request;
  ngx_esp_request_ctx_t *request_context;
};

ngx_esp_request_ctx_s::ngx_esp_request_ctx_s(ngx_http_request_t *r,
                                             ngx_esp_loc_conf_t *lc)
    : current_access_handler(nullptr),
      status(NGX_OK, ""),
      auth_token(ngx_null_string),
      grpc_server_call(nullptr),
      grpc_pass_through(IsGrpcRequest(r)),
      grpc_backend(false),
      backend_time(-1) {
  ngx_memzero(&wakeup_event, sizeof(wakeup_event));
  if (lc && lc->esp) {
    ngx_esp_rewrite_uri(r, lc);

    request_handler = lc->esp->CreateRequestHandler(
        std::unique_ptr<Request>(new NgxEspRequest(r)));

    auto config_id = request_handler->GetServiceConfigId();
    auto it = lc->transcoder_factory_map.find(config_id);
    if (it != lc->transcoder_factory_map.end()) {
      transcoder_factory = it->second;
    } else {
      ::google::protobuf::util::JsonPrintOptions json_print_options =
          ::google::protobuf::util::JsonPrintOptions();
      if (lc->esp->get_always_print_primitive_fields()) {
        json_print_options.always_print_primitive_fields = true;
      }
      transcoder_factory = std::make_shared<transcoding::TranscoderFactory>(
          lc->esp->service(config_id), json_print_options);
      lc->transcoder_factory_map[config_id] = transcoder_factory;
    }
  }
}

ngx_esp_request_ctx_s::~ngx_esp_request_ctx_s() {
  // The client request may be going away before it was woken up
  // by Check continuation. Cancel the wake-up call.
  if (wakeup_context) {
    wakeup_context->request = nullptr;
    wakeup_context->request_context = nullptr;
  }
}

namespace {

// Time in seconds to wait for all active connections to close
// During process exiting.
const int kWaitCloseTime = 3;

// ********************************************************
// * Extensible Service Proxy - Configuration declarations. *
// ********************************************************

//
// Configuration - function declarations.
//

// Creates the module's main context configuration structure.
void *ngx_esp_create_main_conf(ngx_conf_t *cf);

// Initializes the modules' main context configuration structure.
char *ngx_esp_init_main_conf(ngx_conf_t *cf, void *conf);

// Creates the module's location context configuration structure.
void *ngx_esp_create_loc_conf(ngx_conf_t *cf);
char *ngx_esp_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

// Post-configuration initialization.
ngx_int_t ngx_esp_postconfiguration(ngx_conf_t *cf);

// **************************************************
// * Extensible Service Proxy - Runtime declarations. *
// **************************************************

//
// Runtime - function declarations.
//

// The ESP access handler.
ngx_int_t ngx_http_esp_access_wrapper(ngx_http_request_t *r);

// The ESP access handler state machine.
Status ngx_http_esp_access_handler(ngx_http_request_t *r);
Status ngx_http_esp_access_check_done(ngx_http_request_t *r,
                                      ngx_esp_request_ctx_t *ctx);

// The ESP log handler.
ngx_int_t ngx_http_esp_log_handler(ngx_http_request_t *r);

//
// The module commands contain list of configurable properties for this module.
//
ngx_command_t ngx_esp_commands[] = {
    {
        ngx_string("endpoints"),  // name
        NGX_CONF_BLOCK | NGX_CONF_NOARGS | NGX_HTTP_MAIN_CONF |
            NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF,  // type
        ngx_http_endpoints_block,                   // setter
        NGX_HTTP_LOC_CONF_OFFSET,                   // conf
        0,                                          // offset
        nullptr,                                    // void* post
    },
    {
        // grpc_pass defines an nginx request handler that proxies
        // GRPC content (Content-Type "application/grpc*") via
        // libgrpc.
        // The first parameter (if present) defines a gRPC backend address.
        // By default this address is used only when there is no backend
        // configured in service config. If "override" is specified as the
        // second parameter, it is used always regardless of the backend
        // configuration in service config.
        //
        // Usage:
        //   location / {
        //     grpc_pass [<backend_address> [override]];
        //   }
        //
        ngx_string("grpc_pass"),
        NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS | NGX_CONF_TAKE12,
        ConfigureGrpcBackendHandler, NGX_HTTP_LOC_CONF_OFFSET, 0, nullptr,
    },
    {
        ngx_string("endpoints_status"), NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
        ngx_esp_configure_status_handler, NGX_HTTP_LOC_CONF_OFFSET, 0, nullptr,
    },
    {
        ngx_string("endpoints_resolver"), NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        [](ngx_conf_t *cf, ngx_command_t *cmd, void *conf) -> char * {
          return ngx_conf_set_str_slot(
              cf, cmd, &reinterpret_cast<ngx_esp_main_conf_t *>(conf)
                            ->upstream_resolver);
        },
        NGX_HTTP_MAIN_CONF_OFFSET, 0, nullptr,
    },
    {
        ngx_string("endpoints_certificates"),
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
        [](ngx_conf_t *cf, ngx_command_t *cmd, void *conf) -> char * {
          return ngx_conf_set_str_slot(
              cf, cmd,
              &reinterpret_cast<ngx_esp_main_conf_t *>(conf)->cert_path);
        },
        NGX_HTTP_MAIN_CONF_OFFSET, 0, nullptr,
    },
    ngx_null_command  // last entry
};

// Support multiple Nginx variables in ngx_esp_request_ctx_t
// Use this enum to name each variable type.
enum EspVariableType {
  EspVariable_backend_url = 0,
};

ngx_str_t *ngx_esp_request_ctx_variable(ngx_esp_request_ctx_t *ctx,
                                        EspVariableType type) {
  if (type == EspVariable_backend_url) {
    return &ctx->backend_url;
  }
  return nullptr;
}

ngx_int_t ngx_esp_endpoints_variable(ngx_http_request_t *r,
                                     ngx_http_variable_value_t *v,
                                     uintptr_t data) {
  ngx_esp_request_ctx_t *ctx;
  ctx = reinterpret_cast<ngx_esp_request_ctx_t *>(
      ngx_http_get_module_ctx(r, ngx_esp_module));
  if (ctx == nullptr) {
    v->not_found = 1;
    return NGX_OK;
  }
  ngx_str_t *value =
      ngx_esp_request_ctx_variable(ctx, static_cast<EspVariableType>(data));
  if (value == nullptr || value->len <= 0) {
    v->not_found = 1;
    return NGX_OK;
  }
  v->valid = 1;
  v->no_cacheable = 1;
  v->not_found = 0;
  v->len = value->len;
  v->data = value->data;
  return NGX_OK;
}

ngx_http_variable_t ngx_esp_variables[] = {
    {
        ngx_string("backend_url"),                        // name
        nullptr,                                          // set_handler
        ngx_esp_endpoints_variable,                       // get_handler
        static_cast<uintptr_t>(EspVariable_backend_url),  // data
        NGX_HTTP_VAR_NOCACHEABLE | NGX_HTTP_VAR_NOHASH,   // flags
        0,                                                // index
    },
    {ngx_null_string, nullptr, nullptr, 0, 0, 0}  // last entry
};

// Pre-configuration initialization.
ngx_int_t ngx_esp_preconfiguration(ngx_conf_t *cf);

//
// The module context contains initialization and configuration callbacks.
//
ngx_http_module_t ngx_esp_module_ctx = {
    // ngx_int_t (*preconfiguration)(ngx_conf_t *cf);
    ngx_esp_preconfiguration,
    // ngx_int_t (*postconfiguration)(ngx_conf_t *cf);
    ngx_esp_postconfiguration,
    // void *(*create_main_conf)(ngx_conf_t *cf);
    ngx_esp_create_main_conf,
    // char *(*init_main_conf)(ngx_conf_t *cf, void *conf);
    ngx_esp_init_main_conf,
    // void *(*create_srv_conf)(ngx_conf_t *cf);
    nullptr,
    // char *(*merge_srv_conf)(ngx_conf_t *cf, void *prev, void *conf);
    nullptr,
    // void *(*create_loc_conf)(ngx_conf_t *cf);
    ngx_esp_create_loc_conf,
    // char *(*merge_loc_conf)(ngx_conf_t *cf, void *prev, void *conf);
    ngx_esp_merge_loc_conf,
};

//
// Create ESP module's main context configuration
//
void *ngx_esp_create_main_conf(ngx_conf_t *cf) {
  auto *conf =
      RegisterPoolCleanup(cf->pool, new (cf->pool) ngx_esp_main_conf_t);
  if (conf == nullptr) {
    return nullptr;
  }

  // Allowing (by default) for 2 APIs in the server.
  if (ngx_array_init(&conf->endpoints, cf->pool, 2,
                     sizeof(ngx_esp_loc_conf_t *)) != NGX_OK) {
    return nullptr;
  }

  return conf;
}

// Initialize module's main context configuration.
char *ngx_esp_init_main_conf(ngx_conf_t *cf, void *conf) {
  // noop
  return NGX_CONF_OK;
}

//
// Create ESP module's location config.
//
void *ngx_esp_create_loc_conf(ngx_conf_t *cf) {
  auto *lc = new (cf->pool) ngx_esp_loc_conf_t;
  if (lc == nullptr) {
    return nullptr;
  }

  ngx_str_null(&lc->grpc_backend_address_override);
  ngx_str_null(&lc->grpc_backend_address_fallback);
  ngx_str_null(&lc->metadata_server_url);
  ngx_str_null(&lc->service_controller_url);
  ngx_str_null(&lc->cloud_trace_api_url);

  lc->endpoints_api = NGX_CONF_UNSET;
  lc->metadata_server = NGX_CONF_UNSET;
  lc->service_control = NGX_CONF_UNSET;
  lc->cloud_tracing = NGX_CONF_UNSET;
  lc->api_authentication = NGX_CONF_UNSET;

  return lc;
}

// ESP Module pre-configuration. Defines module variables.
//
ngx_int_t ngx_esp_preconfiguration(ngx_conf_t *cf) {
  // Define ESP's variables.
  ngx_http_variable_t *decl;
  for (decl = ngx_esp_variables; decl->name.len > 0; decl++) {
    ngx_http_variable_t *defn;
    defn = ngx_http_add_variable(cf, &decl->name, decl->flags);
    if (defn == nullptr) {
      return NGX_ERROR;
    }
    defn->get_handler = decl->get_handler;
    defn->set_handler = decl->set_handler;
    defn->data = decl->data;
  }
  return NGX_OK;
}

//
// Merge ESP module's location config.
//
char *ngx_esp_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
  ngx_esp_loc_conf_t *prev = reinterpret_cast<ngx_esp_loc_conf_t *>(parent);
  ngx_esp_loc_conf_t *conf = reinterpret_cast<ngx_esp_loc_conf_t *>(child);
  ngx_esp_main_conf_t *mc = reinterpret_cast<ngx_esp_main_conf_t *>(
      ngx_http_conf_get_module_main_conf(cf, ngx_esp_module));

  // API Configuration file name.
  if (conf->endpoints_config.data == nullptr) {
    conf->endpoints_config = prev->endpoints_config;
  }

  ngx_conf_merge_value(conf->endpoints_api, prev->endpoints_api, 0);

  if (conf->google_authentication_secret.data == nullptr) {
    conf->google_authentication_secret = prev->google_authentication_secret;
  }
  if (conf->endpoints_server_config.data == nullptr) {
    conf->endpoints_server_config = prev->endpoints_server_config;
  }

  if (conf->endpoints_api) {
    ngx_esp_loc_conf_t **ploc =
        reinterpret_cast<ngx_esp_loc_conf_t **>(ngx_array_push(&mc->endpoints));
    if (!ploc) return reinterpret_cast<char *>(NGX_CONF_ERROR);
    *ploc = conf;
  }

  conf->http_core_loc_conf = reinterpret_cast<ngx_http_core_loc_conf_t *>(
      ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module));

  ngx_conf_merge_str_value(conf->grpc_backend_address_override,
                           prev->grpc_backend_address_override, nullptr);

  ngx_conf_merge_str_value(conf->grpc_backend_address_fallback,
                           prev->grpc_backend_address_fallback, nullptr);

  if (conf->metadata_server == NGX_CONF_UNSET) {
    conf->metadata_server = prev->metadata_server;
    conf->metadata_server_url = prev->metadata_server_url;
  }

  if (conf->service_control == NGX_CONF_UNSET) {
    conf->service_control = prev->service_control;
    conf->service_controller_url = prev->service_controller_url;
  }

  if (conf->cloud_tracing == NGX_CONF_UNSET) {
    conf->cloud_tracing = prev->cloud_tracing;
    conf->cloud_trace_api_url = prev->cloud_trace_api_url;
  }

  if (conf->api_authentication == NGX_CONF_UNSET) {
    conf->api_authentication = prev->api_authentication;
  }

  return reinterpret_cast<char *>(NGX_CONF_OK);
}

static void handle_endpoints_config_error(ngx_conf_t *cf,
                                          ngx_esp_loc_conf_t *lc) {
  ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                "There were errors with Endpoints api service configuration. ");

  // Disable endpoints
  lc->endpoints_api = 0;
  lc->esp.reset();
}

static ngx_str_t remote_addr = ngx_string("remote_addr");
static ngx_str_t default_cert_name = ngx_string("trusted-ca-certificates.crt");
static ngx_str_t google_dns = ngx_string("8.8.8.8");

//
// Module initialization inserts request handlers into nginx's processing
// pipeline.
//
ngx_int_t ngx_esp_postconfiguration(ngx_conf_t *cf) {
  ngx_esp_main_conf_t *mc = reinterpret_cast<ngx_esp_main_conf_t *>(
      ngx_http_conf_get_module_main_conf(cf, ngx_esp_module));

  mc->remote_addr_variable_index =
      ngx_http_get_variable_index(cf, &remote_addr);

  // Resolve relative path in the configuration directory
  if (mc->cert_path.data == nullptr) {
    mc->cert_path = default_cert_name;
    if (ngx_conf_full_name(cf->cycle, &mc->cert_path, 1) != NGX_OK) {
      ngx_log_error(
          NGX_LOG_WARN, cf->log, 0,
          "Failed to resolve the default trusted CA certificate file.");
      mc->cert_path = ngx_null_string;
    }
  }

  // Check that file exists
  if (mc->cert_path.data != nullptr) {
    if (access((const char *)mc->cert_path.data, R_OK)) {
      ngx_log_error(NGX_LOG_WARN, cf->log, 0,
                    "Failed to open trusted CA certificates file: %V. "
                    "Outgoing HTTPS requests from Endpoints will not check "
                    "server certificates.",
                    &mc->cert_path);
      mc->cert_path = ngx_null_string;
    } else {
      ngx_log_error(NGX_LOG_WARN, cf->log, 0,
                    "Using trusted CA certificates file: %V", &mc->cert_path);
    }
  }

  // Use Google DNS by default
  if (mc->upstream_resolver.data == nullptr) {
    mc->upstream_resolver = google_dns;
  }

  // Set the version to api_manager library.
  utils::Version::instance().set(API_MANAGER_VERSION_STRING);

  bool endpoints_enabled = false;

  ngx_esp_loc_conf_t **endpoints =
      reinterpret_cast<ngx_esp_loc_conf_t **>(mc->endpoints.elts);
  for (ngx_uint_t i = 0, napis = mc->endpoints.nelts; i < napis; i++) {
    ngx_esp_loc_conf_t *lc = endpoints[i];

    if (lc->endpoints_api == 1) {
      ngx_log_t *log = lc->http_core_loc_conf->error_log;
      if (log == nullptr) {
        log = cf->log;
      }

      ngx_resolver_t *resolver = lc->http_core_loc_conf->resolver;
      if (!resolver) {
        ngx_conf_log_error(
            NGX_LOG_EMERG, cf, 0,
            "No resolver defined by the api service configuration file");
        handle_endpoints_config_error(cf, lc);
        return NGX_ERROR;
      }

      // Build the server config based on the esp loc conf to pass to the
      // ApiManager.
      std::string server_config;
      if (ngx_esp_build_server_config(cf, lc, &server_config) != NGX_OK) {
        handle_endpoints_config_error(cf, lc);
        return NGX_ERROR;
      }

      lc->esp = mc->esp_factory.CreateApiManager(
          std::unique_ptr<ApiManagerEnvInterface>(new NgxEspEnv(log)),
          server_config);

      if (!lc->esp) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "Failed to initialize API Management");
        handle_endpoints_config_error(cf, lc);
        return NGX_ERROR;
      }

      // Verify service configs loading status.
      if (!lc->esp->LoadServiceRollouts().ok()) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "Failed to load service configuration files");
        handle_endpoints_config_error(cf, lc);
        return NGX_ERROR;
      }

      // Verify we have service name.
      if (lc->esp->service_name().empty()) {
        ngx_conf_log_error(
            NGX_LOG_EMERG, cf, 0,
            "API service name not specified in configuration file");
        handle_endpoints_config_error(cf, lc);
        return NGX_ERROR;
      }

      endpoints_enabled = endpoints_enabled || lc->esp->Enabled();
    }
  }

  ngx_http_core_main_conf_t *cmcf =
      reinterpret_cast<ngx_http_core_main_conf_t *>(
          ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module));

  // Register the access handler if Endpoints is enabled.
  if (endpoints_enabled) {
    // Access handler.
    ngx_http_handler_pt *h = reinterpret_cast<ngx_http_handler_pt *>(
        ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers));
    if (h == nullptr) {
      ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                         "Cannot install esp check handler.");
      return NGX_ERROR;
    }
    *h = ngx_http_esp_access_wrapper;

    // Log handler
    h = reinterpret_cast<ngx_http_handler_pt *>(
        ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers));
    if (h == nullptr) {
      ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                         "Cannot install esp log handler.");
      return NGX_ERROR;
    }
    *h = ngx_http_esp_log_handler;
  }

  return NGX_OK;
}

// ********************************************************
// * Extensible Service Proxy - Runtime.                    *
// ********************************************************

void wakeup_event_handler(ngx_event_t *ev) {
  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                 "esp: client request wakeup_event_handler %p, r==%p", ev,
                 ev->data);

  ngx_http_request_t *r = reinterpret_cast<ngx_http_request_t *>(ev->data);

  // Wake up the parent request, if any.
  // The parent request is woken up by calling the write_event_handler to
  // kick the NGINX HTTP state machine (at this point, it is likely set to
  // ngx_http_core_run_phases).
  if (r && r->write_event_handler) {
    r->write_event_handler(r);
  }
}

void wakeup_client_request(ngx_http_request_t *r, ngx_esp_request_ctx_t *ctx) {
  // Schedule the wake-up event for the next iteration through the event loop.
  ctx->wakeup_event.data = r;
  ctx->wakeup_event.write = 1;
  ctx->wakeup_event.handler = wakeup_event_handler;
  ctx->wakeup_event.log = r->connection->log;

  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "esp: Posting client request wakeup event %p, request==%p",
                 &ctx->wakeup_event, r);

  // Schedule the wakeup call with NGINX.
  ngx_post_event(&ctx->wakeup_event, &ngx_posted_events);
}

Status ngx_http_esp_access_handler(ngx_http_request_t *r) {
  ngx_esp_loc_conf_t *lc = reinterpret_cast<ngx_esp_loc_conf_t *>(
      ngx_http_get_module_loc_conf(r, ngx_esp_module));

  ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "esp: access handler r=%p, endpoints=%d, esp=%p", r,
                 lc->endpoints_api, lc->esp.get());

  if (!lc->endpoints_api || !lc->esp) {
    return Status(NGX_DECLINED, "Endpoints not configured.");
  }

  if (!lc->esp->Enabled()) {
    return Status(NGX_DECLINED,
                  "Neither Service Control nor Auth are required.");
  }

  // Create per-request context if one doesn't exist already.
  ngx_esp_request_ctx_t *ctx = ngx_http_esp_ensure_module_ctx(r);
  if (ctx == nullptr) {
    return Status(NGX_ERROR, "Missing esp request context.");
  }

  if (ctx->request_handler->ShouldOverrideBackend()) {
    std::string backend_address = ctx->request_handler->GetBackendAddress();
    if (!backend_address.empty()) {
      ngx_str_copy_from_std(r->pool, backend_address, &ctx->backend_url);
      ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                     "esp: use service config backend address: %V",
                     &ctx->backend_url);
    }
  }

  if (ctx->current_access_handler == nullptr) {
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "esp: initiating check, r=%p", r);

    std::shared_ptr<wakeup_context_t> wakeup_context(
        new wakeup_context_t(r, ctx));
    ctx->status = Status(NGX_AGAIN, "Calling check");
    ctx->wakeup_context = wakeup_context;

    ctx->request_handler->Check([wakeup_context](Status status) {
      ngx_http_request_t *r = wakeup_context->request;
      ngx_esp_request_ctx_t *ctx = wakeup_context->request_context;

      // The client request is still around, i.e. it did not timeout.
      if (r != nullptr && ctx != nullptr) {
        ctx->status = status;

        // If the continuation is called within the context of the Check call
        // itself, we haven't yet assigned the current_access_handler below.
        // In that case, we are still inside NGINX event loop and we don't
        // need to wake up the request explicitly. If, however, the current
        // handler is already set, we need to wake up the request to resume
        // NGINX processing.
        if (ctx->current_access_handler != nullptr) {
          wakeup_client_request(r, ctx);
        }

        // Override the backend path if needed, according to service config
        // BackendRule. This is to support backend routing.
        ngx_esp_override_backend_path(r, ctx);
      }
    });

    // Advance the Check call in progress.
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "esp: advance state machine, r=%p", r);

    // Set the continuation handler.
    ctx->current_access_handler = ngx_http_esp_access_check_done;
  }

  return ctx->current_access_handler(r, ctx);
}

Status ngx_http_esp_access_check_done(ngx_http_request_t *r,
                                      ngx_esp_request_ctx_t *ctx) {
  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "Service control check status: %d, error: \"%s\"",
                 ctx->status.code(), ctx->status.message().c_str());
  return ctx->status;
}

ngx_int_t ngx_http_esp_access_wrapper(ngx_http_request_t *r) {
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "esp: access wrapper r=%p", r);
  Status status = ngx_http_esp_access_handler(r);
  if (!status.ok()) {
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "esp: access wrapper status: r=%p: %s", r,
                   status.ToString().c_str());

    // Request fails check step
    if (status.code() > 0 || status.code() == NGX_ERROR) {
      ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                     "esp: access wrapper finalizing r=%p", r);

      // status is passed by context
      ngx_http_finalize_request(r, ngx_esp_return_error(r));
      return NGX_DONE;
    }
  } else {
    // If check is successful, update cause of error to application backend
    ngx_esp_request_ctx_t *ctx = reinterpret_cast<ngx_esp_request_ctx_t *>(
        ngx_http_get_module_ctx(r, ngx_esp_module));

    if (ctx != nullptr) {
      ctx->status = Status(Code::OK, "", Status::APPLICATION);
    }
  }

  return status.code();
}

ngx_int_t ngx_http_esp_log_handler(ngx_http_request_t *r) {
  ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "esp log handler: enter: r=%p, i=%d, %V", r, r->internal,
                 &r->uri);

  if (r->internal) {
    // Ignore internal requests.
    return NGX_OK;
  }

  ngx_esp_loc_conf_t *lc = reinterpret_cast<ngx_esp_loc_conf_t *>(
      ngx_http_get_module_loc_conf(r, ngx_esp_module));
  if (lc == nullptr || !lc->endpoints_api || lc->esp == nullptr) {
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "esp: skip r=%p (not endpoints)", r);
    // Not an Endpoints API.
    return NGX_OK;
  }

  ngx_esp_request_ctx_t *ctx = reinterpret_cast<ngx_esp_request_ctx_t *>(
      ngx_http_get_module_ctx(r, ngx_esp_module));
  if (ctx == nullptr) {
    return NGX_OK;
  }

  if (ctx->request_handler) {
    ctx->request_handler->Report(
        std::unique_ptr<Response>(new NgxEspResponse(r)), []() {});
  }
  return NGX_OK;
}

// Creates HTTP configuration tree to use for HTTP request implementation
// based on NGINX upstream module.
//
// The goal is to create configuration tree similar to that created by
// an empty http { ... } configuration block.
ngx_int_t ngx_esp_create_module_configurations(ngx_http_conf_ctx_t *ctx,
                                               ngx_conf_t *cf) {
  ctx->main_conf = reinterpret_cast<void **>(
      ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module));
  if (ctx->main_conf == nullptr) {
    return NGX_ERROR;
  }

  ctx->srv_conf = reinterpret_cast<void **>(
      ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module));
  if (ctx->srv_conf == nullptr) {
    return NGX_ERROR;
  }

  ctx->loc_conf = reinterpret_cast<void **>(
      ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module));
  if (ctx->loc_conf == nullptr) {
    return NGX_ERROR;
  }

  // Iterate through all modules.
  for (int i = 0;; i++) {
    ngx_module_t *m = cf->cycle->modules[i];

    // Last module, we are done.
    if (!m) {
      break;
    }

    // Ignore non-HTTP modules.
    if (m->type != NGX_HTTP_MODULE) {
      continue;
    }

    ngx_http_module_t *module = reinterpret_cast<ngx_http_module_t *>(m->ctx);
    auto mi = m->ctx_index;

    // Create module's main configuration.
    if (module->create_main_conf) {
      ctx->main_conf[mi] = module->create_main_conf(cf);
      if (ctx->main_conf[mi] == nullptr) {
        return NGX_ERROR;
      }
    }

    // Server configuration.
    if (module->create_srv_conf) {
      ctx->srv_conf[mi] = module->create_srv_conf(cf);
      if (ctx->srv_conf[mi] == nullptr) {
        return NGX_ERROR;
      }
    }

    // Local configuration.
    if (module->create_loc_conf) {
      ctx->loc_conf[mi] = module->create_loc_conf(cf);
      if (ctx->loc_conf[mi] == nullptr) {
        return NGX_ERROR;
      }
    }
  }

  return NGX_OK;
}

ngx_int_t ngx_esp_create_http_configuration(ngx_conf_t *cf,
                                            ngx_esp_main_conf_t *mc) {
  // Create location configs that we use for outgoing requests.
  void **location_configs = reinterpret_cast<void **>(
      ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module));
  if (!location_configs) {
    return NGX_ERROR;
  }

  for (int i = 0;; i++) {
    ngx_module_t *m = cf->cycle->modules[i];

    if (!m) {
      break;
    }

    if (m->type != NGX_HTTP_MODULE) {
      continue;
    }

    ngx_http_conf_ctx_t *ctx = reinterpret_cast<ngx_http_conf_ctx_t *>(cf->ctx);
    ngx_http_module_t *module = reinterpret_cast<ngx_http_module_t *>(m->ctx);
    auto mi = m->ctx_index;

    if (module->create_loc_conf) {
      location_configs[mi] = module->create_loc_conf(cf);
      if (!location_configs[mi]) {
        // Location config merge failed.
        ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                      "Failed to create local config.");
        return NGX_ERROR;
      }

      if (module->merge_loc_conf) {
        char *result =
            module->merge_loc_conf(cf, ctx->loc_conf[mi], location_configs[mi]);
        if (result != NGX_CONF_OK) {
          // Location config merge failed.
          ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                        "Failed to merge local config.");
          return NGX_ERROR;
        }
      }
    }
  }

  // Create a resolver.
  auto clcf = reinterpret_cast<ngx_http_core_loc_conf_t *>(
      location_configs[ngx_http_core_module.ctx_index]);
  ngx_str_t dns_resolver = mc->upstream_resolver;
  ngx_str_t names[] = {
      {dns_resolver.len, ngx_pstrdup(cf->pool, &dns_resolver)}};
  if (names[0].data == nullptr) {
    return NGX_ERROR;
  }

  clcf->resolver = ngx_resolver_create(cf, names, 1);
  if (clcf->resolver == nullptr) {
    ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                  "Failed to create an Endpoints DNS resolver.");
    return NGX_ERROR;
  }

#if (NGX_HAVE_INET6)
  // ipv6 is not supported in Google Compute Engine so if we resolve
  // DNS names and include ipv6, NGINX will rotate through all returned
  // IP addresses, including the ipv6 ones, and any connection made to
  // an ipv6 address will fail.
  //
  // TODO: make this configurable.
  clcf->resolver->ipv6 = 0;
#endif

  // Make sure that Endpoints is disabled in this set of module configs.
  auto lc = reinterpret_cast<ngx_esp_loc_conf_t *>(
      location_configs[ngx_esp_module.ctx_index]);
  lc->endpoints_api = 0;
  lc->endpoints_config = ngx_null_string;
  lc->esp.reset();

  mc->http_module_conf_ctx.loc_conf = location_configs;

#if NGX_HTTP_SSL
  // Initialize SSL
  auto ssl_cleanup = ngx_pool_cleanup_add(cf->pool, sizeof(ngx_ssl_t));
  if (ssl_cleanup == nullptr) {
    return NGX_ERROR;
  }
  ngx_ssl_t *ssl = reinterpret_cast<ngx_ssl_t *>(ssl_cleanup->data);
  ngx_memzero(ssl, sizeof(ngx_ssl_t));
  ssl->log = cf->log;

  if (ngx_ssl_create(ssl, NGX_SSL_SSLv2 | NGX_SSL_SSLv3 | NGX_SSL_TLSv1 |
                              NGX_SSL_TLSv1_1 | NGX_SSL_TLSv1_2,
                     nullptr) != NGX_OK) {
    return NGX_ERROR;
  }

  // Loads trusted CA certificates. All outgoing HTTPS requests will
  // require proper server certificates.
  if (mc->cert_path.len > 0 &&
      SSL_CTX_load_verify_locations(ssl->ctx, (const char *)mc->cert_path.data,
                                    nullptr) == 0) {
    return NGX_ERROR;
  }

  ssl_cleanup->handler = ngx_ssl_cleanup_ctx;
  mc->ssl = ssl;
#endif

  return NGX_OK;
}

// ESP module initialization.
ngx_int_t ngx_esp_init_module(ngx_cycle_t *cycle) {
  auto mc = reinterpret_cast<ngx_esp_main_conf_t *>(
      ngx_http_cycle_get_module_main_conf(cycle, ngx_esp_module));

  // Endpoints is not enabled (or http { ... } block is missing altogether).
  if (mc == nullptr) {
    return NGX_OK;
  }

  // The triple of HTTP configuration contexts: main, server, local.
  ngx_http_conf_ctx_t http_conf_ctx = {nullptr, nullptr, nullptr};

  // Configuration state.
  ngx_conf_t conf;
  ngx_memzero(&conf, sizeof(ngx_conf_t));
  conf.ctx = &http_conf_ctx;
  conf.cycle = cycle;
  conf.pool = cycle->pool;
  conf.log = cycle->log;
  conf.module_type = NGX_HTTP_MODULE;
  conf.cmd_type = NGX_HTTP_MAIN_CONF;

  // Fixed crash with v1.15.0
  conf.conf_file =
      (ngx_conf_file_t *)ngx_pcalloc(cycle->pool, sizeof(ngx_conf_file_t));
  if (conf.conf_file == NULL) {
    ngx_log_error(NGX_LOG_EMERG, conf.log, 0,
                  "Failed to allocate memory for conf_file object.");
    return NGX_ERROR;
  }
  conf.conf_file->file.name = cycle->conf_file;

  // Create a temporary pool (destroyed below).
  conf.temp_pool = ngx_create_pool(NGX_CYCLE_POOL_SIZE, conf.log);
  if (conf.temp_pool == nullptr) {
    return NGX_ERROR;
  }

  ngx_int_t result = NGX_ERROR;

  // Create the http configuration tree.
  if (ngx_esp_create_module_configurations(&http_conf_ctx, &conf) != NGX_OK) {
    ngx_log_error(
        NGX_LOG_EMERG, cycle->log, 0,
        "Cannot create module configurations for endpoints http requests.");
    goto done;
  }

  // Create (and merge) local configurations. The local configuration
  // context array is what the upstream-based HTTP implementation uses.
  if (ngx_esp_create_http_configuration(&conf, mc) != NGX_OK) {
    ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                  "Cannot create http module local configurations.");
    goto done;
  }

  result = NGX_OK;

done:
  // Destroy temporary pool created above.
  if (conf.temp_pool != nullptr) {
    ngx_destroy_pool(conf.temp_pool);
  }
  return result;
}

ngx_uint_t ngx_esp_count_active_connections(ngx_cycle_t *cycle) {
  ngx_uint_t i, active = 0;
  ngx_connection_t *c;

  c = cycle->connections;
  for (i = 0; i < cycle->connection_n; i++) {
    if (c[i].fd != -1 && c[i].read && !c[i].read->accept &&
        !c[i].read->channel && !c[i].read->resolver) {
      ++active;
    }
  }
  return active;
}

// Attempts to shut down ESP, returns true if ESP is ready to exit.
bool ngx_esp_attempt_shutdown(ngx_cycle_t *cycle) {
  // The states ESP needs to go throught before it is ready to exit:
  // 1) Waits all current requests to be done before closing ESP service
  //    control. This wait should not be long since when exit_handler
  //    is called, all listen ports have been closed.
  // 2) Closes ESP service control to flush out cached items.
  // 3) Waits for requests sending these cached items to be done.
  ngx_esp_main_conf_t *mc = reinterpret_cast<ngx_esp_main_conf_t *>(
      ngx_http_cycle_get_module_main_conf(cycle, ngx_esp_module));

  if (mc->exit_wait_start_time == 0) {
    mc->exit_wait_start_time = ngx_time();
  }
  if (ngx_esp_count_active_connections(cycle) > 0 &&
      ngx_time() - mc->exit_wait_start_time < kWaitCloseTime) {
    return false;
  }
  if (mc->esp_closed) {
    return true;
  }

  // Closes ESP service control to flush out its cached items.
  ngx_esp_loc_conf_t **endpoints =
      reinterpret_cast<ngx_esp_loc_conf_t **>(mc->endpoints.elts);
  for (ngx_uint_t i = 0, napis = mc->endpoints.nelts; i < napis; i++) {
    ngx_esp_loc_conf_t *lc = endpoints[i];
    if (lc->endpoints_api == 1 && lc->esp) {
      lc->esp->Close();
    }
  }
  mc->exit_wait_start_time = 0;
  mc->esp_closed = true;
  return false;
}

// The shutdown timeout.
// The module schedules a short timeout to detect NGINX shutdown.
//
ngx_msec_t shutdown_timeout = 500;

// Forward declaration.
void ngx_http_esp_exit_timer_event_handler(ngx_event_t *ev);

void ngx_esp_schedule_exit_timer(ngx_cycle_t *cycle, ngx_event_t *ev) {
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
                 "Scheduling shutdown timeout in %M ms", shutdown_timeout);

  ngx_memzero(ev, sizeof(ngx_event_t));

  // TODO: NGINX (in debug mode) assumes that ev->data is ngx_connection_t*
  ev->data = cycle;
  ev->handler = ngx_http_esp_exit_timer_event_handler;
  ev->log = cycle->log;

  ngx_add_timer(ev, shutdown_timeout);
}

void ngx_http_esp_exit_timer_event_handler(ngx_event_t *ev) {
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                 "esp: exit timer handler: timedout=%d", ev->timedout);

  ngx_cycle_t *cycle = reinterpret_cast<ngx_cycle_t *>(ev->data);

  if (!ngx_exiting && !ngx_terminate && !ngx_quit) {
    ngx_esp_schedule_exit_timer(cycle, ev);
  } else if (!ngx_esp_attempt_shutdown(cycle)) {
    // The shutdown attempt hasn't succeeded yet. Reschedule timer again
    // with shorter timeout to give shutdown more time to complete.
    shutdown_timeout = 10;
    ngx_esp_schedule_exit_timer(cycle, ev);
  }
}

ngx_int_t ngx_esp_init_process(ngx_cycle_t *cycle) {
  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0, "ngx_esp_init_process");
  // Nginx timers are initialized only after configuration is loaded
  // (in core module process init) so we must create our timers here
  // (or later).

  ngx_esp_main_conf_t *mc = reinterpret_cast<ngx_esp_main_conf_t *>(
      ngx_http_cycle_get_module_main_conf(cycle, ngx_esp_module));
  if (!mc) {
    // Handle the case where there is no http section at all.
    return NGX_OK;
  }
  bool has_esp = false;
  ngx_esp_loc_conf_t **endpoints =
      reinterpret_cast<ngx_esp_loc_conf_t **>(mc->endpoints.elts);
  for (ngx_uint_t i = 0, napis = mc->endpoints.nelts; i < napis; i++) {
    ngx_esp_loc_conf_t *lc = endpoints[i];

    if (lc->endpoints_api == 1 && lc->esp) {
      lc->esp->Init();
      has_esp = true;
    }
    if (lc->grpc_pass && !mc->grpc_queue) {
      mc->grpc_queue = NgxEspGrpcQueue::Instance();
      mc->grpc_queue->Init(cycle);
    }
  }

  if (mc->stats_zone != nullptr) {
    ngx_int_t rc = ngx_esp_init_process_stats(cycle);
    if (rc != NGX_OK) {
      return rc;
    }
  }

  // Only if Endpoints is enabled.
  if (has_esp) {
    // Registers an event with a very long timeout in order to detect when NGINX
    // is exiting. During NGINX shutdown, timers will be canceled and the exit
    // timer handler will be called, giving it a chance to flush caches.

    ngx_esp_schedule_exit_timer(cycle, &mc->exit_timer);

    mc->exit_wait_start_time = 0;
    mc->esp_closed = false;
  }

  return NGX_OK;
}

void ngx_esp_exit_process(ngx_cycle_t *cycle) {
  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0, "ngx_esp_exit_process");

  ngx_esp_main_conf_t *mc = reinterpret_cast<ngx_esp_main_conf_t *>(
      ngx_http_cycle_get_module_main_conf(cycle, ngx_esp_module));
  if (!mc) {
    // Handle the case where there is no http section at all.
    return;
  }
  ngx_esp_loc_conf_t **endpoints =
      reinterpret_cast<ngx_esp_loc_conf_t **>(mc->endpoints.elts);
  for (ngx_uint_t i = 0, napis = mc->endpoints.nelts; i < napis; i++) {
    ngx_esp_loc_conf_t *lc = endpoints[i];

    lc->~ngx_esp_loc_conf_t();
  }
}
}  // namespace

ngx_esp_request_ctx_t *ngx_http_esp_ensure_module_ctx(ngx_http_request_t *r) {
  // Create per-request context if one doesn't exist already.
  ngx_esp_request_ctx_t *ctx = reinterpret_cast<ngx_esp_request_ctx_t *>(
      ngx_http_get_module_ctx(r, ngx_esp_module));

  ngx_esp_loc_conf_t *lc = reinterpret_cast<ngx_esp_loc_conf_t *>(
      ngx_http_get_module_loc_conf(r, ngx_esp_module));

  if (ctx == nullptr) {
    ctx = RegisterPoolCleanup(r->pool,
                              new (r->pool) ngx_esp_request_ctx_t(r, lc));
    if (ctx != nullptr) {
      ngx_http_set_ctx(r, ctx, ngx_esp_module);
    }
  }

  return ctx;
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

//
// The module definition, referenced from the 'config' file.
// N.B. This is the definition that's referenced by the nginx sources;
// it must be globally scoped.
//
ngx_module_t ngx_esp_module = {
    NGX_MODULE_V1,                                      // v1 module type
    &::google::api_manager::nginx::ngx_esp_module_ctx,  // ctx
    ::google::api_manager::nginx::ngx_esp_commands,     // commands
    NGX_HTTP_MODULE,                                    // type

    // ngx_int_t (*init_master)(ngx_log_t *log)
    nullptr,
    // ngx_int_t (*init_module)(ngx_cycle_t *cycle);
    ::google::api_manager::nginx::ngx_esp_init_module,
    // ngx_int_t (*init_process)(ngx_cycle_t *cycle);
    ::google::api_manager::nginx::ngx_esp_init_process,
    // ngx_int_t (*init_thread)(ngx_cycle_t *cycle);
    nullptr,
    // void (*exit_thread)(ngx_cycle_t *cycle);
    nullptr,
    // void (*exit_process)(ngx_cycle_t *cycle);
    ::google::api_manager::nginx::ngx_esp_exit_process,
    // void (*exit_master)(ngx_cycle_t *cycle);
    nullptr,

    NGX_MODULE_V1_PADDING  // padding the rest of the ngx_module_t structure
};
