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
// An Endpoints Server Proxy GRPC nginx module.
//

#include <deque>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include <ngx_core.h>
#include <ngx_module.h>
#include <ngx_string.h>
}

#include "include/api_manager/env_interface.h"
#include "src/api_manager/grpc/server.h"
#include "src/api_manager/grpc/server_builder.h"
#include "src/nginx/alloc.h"
#include "src/nginx/config.h"
#include "src/nginx/environment.h"
#include "src/nginx/module.h"
#include "src/nginx/util.h"

using ::google::api_manager::utils::Status;
using ::google::api_manager::nginx::ngx_str_to_std;
using ::google::api_manager::nginx::ngx_str_copy_from_std;

extern ngx_module_t ngx_grpc_module;

namespace google {
namespace api_manager {
namespace nginx {
namespace grpc {

const ngx_uint_t kGrpcModuleType = 0x43505247; /* "GRPC", in little-endian */
const ngx_uint_t kGrpcServerConf = 0x80000000;
const ngx_str_t kNoAPI = ngx_string("(none)");

struct ServerConfig {
  ServerConfig *next = nullptr;
  ngx_str_t listen_address = ngx_null_string;
  ngx_str_t backend_override_address = ngx_null_string;
  ngx_str_t service_config_path = ngx_null_string;
  ngx_int_t call_limit = NGX_CONF_UNSET;
  ngx_str_t ssl_root_certificates = ngx_null_string;
  ngx_str_t ssl_certificate_key = ngx_null_string;
  ngx_str_t ssl_certificate = ngx_null_string;
  ngx_flag_t ssl_force_client_auth = NGX_CONF_UNSET;
};

struct ModuleConfig {
  ServerConfig *server_configs = nullptr;
  std::vector<std::shared_ptr<api_manager::grpc::Server>> servers;
};

char *ConfigureBlock(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
  // Count the number of the grpc modules and set up their indices.
  ngx_count_modules(cf->cycle, kGrpcModuleType);

  ModuleConfig *config = *reinterpret_cast<ModuleConfig **>(conf);
  ServerConfig *server = new (cf->pool) ServerConfig;
  server->next = config->server_configs;
  config->server_configs = server;

  ngx_uint_t prev_module_type = cf->module_type;
  ngx_uint_t prev_cmd_type = cf->cmd_type;
  void *prev_ctx = cf->ctx;

  cf->module_type = kGrpcModuleType;
  cf->cmd_type = kGrpcServerConf;
  cf->ctx = &config;
  char *rv = ngx_conf_parse(cf, nullptr);

  cf->module_type = prev_module_type;
  cf->cmd_type = prev_cmd_type;
  cf->ctx = prev_ctx;

  if (rv != NGX_CONF_OK) {
    return rv;
  }
  if (!server->listen_address.data) {
    return const_cast<char *>("listen address not specified");
  }
  if (server->call_limit != NGX_CONF_UNSET && server->call_limit <= 0) {
    return const_cast<char *>("invalid GRPC server call limit (must be > 0)");
  }
  return NGX_CONF_OK;
}

// clang-format off
// The commands that are valid at the top-level configuration scope.
ngx_command_t core_commands[] = {
  {
    ngx_string("grpc"),
    NGX_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
    &ConfigureBlock,
    0,
    0,
    nullptr
  },
  ngx_null_command
};

// The commands that are valid within a grpc{} block for configuring a
// GRPC server.
ngx_command_t server_commands[] = {
  {
    ngx_string("listen"),
    kGrpcServerConf|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    0,
    offsetof(ServerConfig, listen_address),
    nullptr
  },
  {
    ngx_string("backend_override"),
    kGrpcServerConf|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    0,
    offsetof(ServerConfig, backend_override_address),
    nullptr
  },
  {
    ngx_string("api"),
    kGrpcServerConf|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    0,
    offsetof(ServerConfig, service_config_path),
    nullptr
  },
  {
    ngx_string("call_limit"),
    kGrpcServerConf|NGX_CONF_TAKE1,
    ngx_conf_set_num_slot,
    0,
    offsetof(ServerConfig, call_limit),
    nullptr
  },
  {
    ngx_string("ssl_root_certificates"),
    kGrpcServerConf|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    0,
    offsetof(ServerConfig, ssl_root_certificates),
    nullptr
  },
  {
    ngx_string("ssl_certificate_key"),
    kGrpcServerConf|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    0,
    offsetof(ServerConfig, ssl_certificate_key),
    nullptr
  },
  {
    ngx_string("ssl_certificate"),
    kGrpcServerConf|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    0,
    offsetof(ServerConfig, ssl_certificate),
    nullptr
  },
  {
    ngx_string("ssl_force_client_auth"),
    kGrpcServerConf|NGX_CONF_TAKE1,
    ngx_conf_set_flag_slot,
    0,
    offsetof(ServerConfig, ssl_force_client_auth),
    nullptr
  },
  ngx_null_command
};
// clang-format on

void *CreateConf(ngx_cycle_t *cycle) {
  ModuleConfig *conf = new (cycle->pool) ModuleConfig();
  return conf;
}

// A class to implement grpc::CallHandler interface.
// It will jump the thread and call the ESP code in the Nginx thread.
class NgxEspGrpcCallHandler : public api_manager::grpc::CallHandler {
 public:
  NgxEspGrpcCallHandler(std::shared_ptr<ApiManager> esp) : esp_(esp) {}

  virtual void Check(
      std::unique_ptr<Request> request,
      std::function<void(const std::string &, Status)> continuation) {
    handler_ = esp_->CreateRequestHandler(std::move(request));
    PostCallback([this, continuation]() {
      std::string backend_address = handler_->GetBackendAddress();
      handler_->Check([backend_address, continuation](utils::Status status) {
        continuation(backend_address, status);
      });
    });
  }

  virtual void Report(std::unique_ptr<Response> response,
                      std::function<void(void)> continuation) {
    // Lambda capture doens't support std::unique_ptr, use raw pointer.
    auto raw_response = response.release();
    PostCallback([this, raw_response, continuation]() {
      handler_->Report(std::unique_ptr<Response>(raw_response), continuation);
    });
  }

 private:
  static std::mutex nginx_queue_mutex;
  static std::deque<std::function<void()>> nginx_queue;

  static void PostCallback(std::function<void()> cb) {
    bool notify_nginx = false;
    {
      std::lock_guard<std::mutex> lock(nginx_queue_mutex);
      if (nginx_queue.size() == 0) {
        notify_nginx = true;
      }
      nginx_queue.emplace_back(cb);
    }
    if (notify_nginx) {
      ngx_notify(&QueueHandler);
    }
  }

  static void QueueHandler(ngx_event_t *) {
    std::deque<std::function<void()>> q;
    {
      std::lock_guard<std::mutex> lock(nginx_queue_mutex);
      q.swap(nginx_queue);
    }
    for (auto &cb : q) {
      cb();
    }
  }

  std::shared_ptr<ApiManager> esp_;
  std::unique_ptr<RequestHandlerInterface> handler_;
};

std::mutex NgxEspGrpcCallHandler::nginx_queue_mutex;
std::deque<std::function<void()>> NgxEspGrpcCallHandler::nginx_queue;

ngx_int_t ReadFileContents(ngx_cycle_t *cycle, ngx_str_t name,
                           std::string *contents) {
  if (ngx_conf_full_name(cycle, &name, 1) != NGX_OK) {
    return NGX_ERROR;
  }
  std::string strname = ngx_str_to_std(name);
  std::ifstream ifs(strname, std::ios::binary);
  if (!ifs) {
    return NGX_ERROR;
  }
  std::ostringstream buffer;
  buffer << ifs.rdbuf();
  *contents = buffer.str();
  return NGX_OK;
}

ngx_int_t InitProcess(ngx_cycle_t *cycle) {
  ModuleConfig *config =
      reinterpret_cast<ModuleConfig *>(cycle->conf_ctx[ngx_grpc_module.index]);
  if (!config || !config->server_configs) {
    return NGX_OK;
  }
  for (ServerConfig *server_config = config->server_configs; server_config;
       server_config = server_config->next) {
    std::shared_ptr<ApiManager> esp;

    if (server_config->service_config_path.data) {
      ngx_str_t file_name = server_config->service_config_path;
      ngx_str_t file_contents = ngx_null_string;
      if (!(ngx_conf_full_name(cycle, &file_name, 1) == NGX_OK &&
            nginx::ngx_esp_read_file((const char *)file_name.data, cycle->pool,
                                     &file_contents) == NGX_OK)) {
        // log the error and continue with nginx startup
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "gRPC server listen=%V cannot initialize and will "
                      "be disabled. Failed to find api service configuration "
                      "file : %V",
                      &server_config->listen_address, &file_name);
        continue;
      }

      auto *mc = reinterpret_cast<ngx_esp_main_conf_t *>(
          ngx_http_cycle_get_module_main_conf(cycle, ngx_esp_module));
      if (mc) {
        esp = mc->esp_factory.GetOrCreateApiManager(
            std::unique_ptr<ApiManagerEnvInterface>(
                new NgxEspEnv(cycle->log, nullptr)),
            ngx_str_to_std(file_contents), "");
        if (!esp) {
          // log the error and continue with nginx startup
          ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                        "gRPC server listen=%V cannot initialize and will "
                        "be disabled. Failed to initalize Endpoints from "
                        "\"%V\" service configuration file.",
                        &server_config->listen_address, &file_name);
          continue;
        }
      }
    }

    api_manager::grpc::ServerBuilder builder;

    if (server_config->call_limit != NGX_CONF_UNSET) {
      builder.SetMaxParallelCalls(server_config->call_limit);
    }

    ngx_log_error(
        NGX_LOG_DEBUG, cycle->log, 0,
        "Starting GRPC proxy: listen=\"%V\" override=\"%V\" api=\"%V\" "
        "call_maximum=%d",
        &server_config->listen_address,
        &server_config->backend_override_address,
        server_config->service_config_path.data
            ? &server_config->service_config_path
            : &kNoAPI,
        builder.GetMaxParallelCalls());

    std::shared_ptr<::grpc::ServerCredentials> creds =
        ::grpc::InsecureServerCredentials();

    if (server_config->ssl_certificate.data) {
      ::grpc::SslServerCredentialsOptions opts;
      if (server_config->ssl_force_client_auth == 1) {
        opts.force_client_auth = true;
      }
      if (server_config->ssl_root_certificates.data) {
        if (ReadFileContents(cycle, server_config->ssl_root_certificates,
                             &opts.pem_root_certs) != NGX_OK) {
          ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                        "Unable to read the root certificate store at \"%V\".",
                        &server_config->ssl_root_certificates);
          return NGX_ERROR;
        }
      }
      if (server_config->ssl_certificate.data) {
        ::grpc::SslServerCredentialsOptions::PemKeyCertPair pair;
        if (ReadFileContents(cycle, server_config->ssl_certificate,
                             &pair.cert_chain) != NGX_OK) {
          ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                        "Unable to read the server certificate at \"%V\".",
                        &server_config->ssl_certificate);
          return NGX_ERROR;
        }
        if (!server_config->ssl_certificate_key.data) {
          ngx_log_error(NGX_LOG_WARN, cycle->log, 0,
                        "No server certificate key specified.");
        } else {
          if (ReadFileContents(cycle, server_config->ssl_certificate_key,
                               &pair.private_key) != NGX_OK) {
            ngx_log_error(
                NGX_LOG_EMERG, cycle->log, 0,
                "Unable to read the server certificate key at \"%V\".",
                &server_config->ssl_certificate_key);
            return NGX_ERROR;
          }
        }
        opts.pem_key_cert_pairs.emplace_back(std::move(pair));
      }
      creds = ::grpc::SslServerCredentials(opts);
    }

    builder.AddListeningPort(ngx_str_to_std(server_config->listen_address),
                             creds);
    if (esp) {
      auto *mc = reinterpret_cast<ngx_esp_main_conf_t *>(
          ngx_http_cycle_get_module_main_conf(cycle, ngx_esp_module));
      if (!mc) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "Configuration error: GRPC proxy with Endpoints "
                      "requires a separate"
                      " http{} configuration block with endpoints enabled.");
        return NGX_ERROR;
      }
      builder.SetCallHandlerFactory([esp]() {
        return std::unique_ptr<api_manager::grpc::CallHandler>(
            new NgxEspGrpcCallHandler(esp));
      });
    }
    if (server_config->backend_override_address.data) {
      builder.SetBackendOverride(
          ngx_str_to_std(server_config->backend_override_address));
    }
    // TODO: Investigate what happens in a configuration failure here,
    // e.g. someone creates two GRPC servers on the same port.
    config->servers.emplace_back(builder.BuildAndRun());
  }

  return NGX_OK;
}

void ExitProcess(ngx_cycle_t *cycle) {
  ModuleConfig *config =
      reinterpret_cast<ModuleConfig *>(cycle->conf_ctx[ngx_grpc_module.index]);
  if (config) {
    for (auto &server : config->servers) {
      server->StartShutdown();
    }
    for (auto &server : config->servers) {
      server->WaitForShutdown();
    }
    config->~ModuleConfig();
  }
}

ngx_core_module_t module_ctx = {
    ngx_string("grpc"),
    // void *(*create_conf)(ngx_cycle_t *cycle)
    &CreateConf,
    // char *(*init_conf)(ngx_cycle_t *cycle, void *conf)
    nullptr};

}  // namespace grpc
}  // namespace nginx
}  // namespace api_manager
}  // namespace google

//
// The module definitions, referenced from the 'config' file.
// N.B. These definitions are referenced by the nginx sources;
// they must be globally scoped.
//
// clang-format off
ngx_module_t ngx_grpc_module = {
    NGX_MODULE_V1,                      // v1 module type
    &::google::api_manager::nginx::grpc::module_ctx,    // ctx
    ::google::api_manager::nginx::grpc::core_commands,  // commands
    NGX_CORE_MODULE,                    // type

    // ngx_int_t (*init_master)(ngx_log_t *log)
    nullptr,
    // ngx_int_t (*init_module)(ngx_cycle_t *cycle);
    nullptr,
    // ngx_int_t (*init_process)(ngx_cycle_t *cycle);
    &::google::api_manager::nginx::grpc::InitProcess,
    // ngx_int_t (*init_thread)(ngx_cycle_t *cycle);
    nullptr,
    // void (*exit_thread)(ngx_cycle_t *cycle);
    nullptr,
    // void (*exit_process)(ngx_cycle_t *cycle);
    &::google::api_manager::nginx::grpc::ExitProcess,
    // void (*exit_master)(ngx_cycle_t *cycle);
    nullptr,

    NGX_MODULE_V1_PADDING  // padding the rest of the ngx_module_t structure
};

ngx_module_t ngx_grpc_server_module = {
    NGX_MODULE_V1,                       // v1 module type
    nullptr,                                // ctx
    ::google::api_manager::nginx::grpc::server_commands, // commands
    ::google::api_manager::nginx::grpc::kGrpcModuleType, // type

    // ngx_int_t (*init_master)(ngx_log_t *log)
    nullptr,
    // ngx_int_t (*init_module)(ngx_cycle_t *cycle);
    nullptr,
    // ngx_int_t (*init_process)(ngx_cycle_t *cycle);
    nullptr,
    // ngx_int_t (*init_thread)(ngx_cycle_t *cycle);
    nullptr,
    // void (*exit_thread)(ngx_cycle_t *cycle);
    nullptr,
    // void (*exit_process)(ngx_cycle_t *cycle);
    nullptr,
    // void (*exit_master)(ngx_cycle_t *cycle);
    nullptr,

    NGX_MODULE_V1_PADDING  // padding the rest of the ngx_module_t structure
};
// clang-format on
