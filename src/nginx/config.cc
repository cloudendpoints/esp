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
#include "src/nginx/config.h"

#include <fcntl.h>
#include <string>

#include "contrib/endpoints/src/api_manager/proto/server_config.pb.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/type_resolver.h"
#include "google/protobuf/util/type_resolver_util.h"
#include "src/nginx/module.h"
#include "src/nginx/status.h"
#include "src/nginx/util.h"

namespace google {
namespace api_manager {
namespace nginx {

using ::google::api_manager::proto::ServerConfig;

namespace {

// Allocates extra_bytes more and initializes them with '\0'.
ngx_int_t ngx_esp_read_file_impl(const char *filename, ngx_pool_t *pool,
                                 ngx_str_t *data, size_t extra_bytes) {
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    return NGX_ERROR;
  }

  data->data = 0;
  data->len = 0;

  size_t file_size = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);

  data->data =
      reinterpret_cast<u_char *>(ngx_pcalloc(pool, file_size + extra_bytes));
  if (data->data == nullptr) {
    return NGX_ERROR;
  }

  ssize_t read_size = read(fd, data->data, file_size);
  if (read_size == -1) {
    return NGX_ERROR;
  }

  data->len = (size_t)read_size;
  return NGX_OK;
}

// ESP configuration parser handlers.

const char *esp_enable_endpoints(ngx_conf_t *cf, ngx_esp_loc_conf_t *conf) {
  if (conf->endpoints_api == NGX_CONF_UNSET) {
    conf->endpoints_api = 1;
    return NGX_CONF_OK;
  }
  return (conf->endpoints_api == 1) ? "duplicate" : "conflicting";
}

const char *esp_disable_endpoints(ngx_conf_t *cf, ngx_esp_loc_conf_t *conf) {
  if (conf->endpoints_api == NGX_CONF_UNSET) {
    conf->endpoints_api = 0;
    return NGX_CONF_OK;
  }
  return (conf->endpoints_api == 0) ? "duplicate" : "conflicting";
}

const char *esp_set_endpoints_config(ngx_conf_t *cf, ngx_esp_loc_conf_t *conf) {
  ngx_str_t *argv = reinterpret_cast<ngx_str_t *>(cf->args->elts);
  if (conf->endpoints_config.data == nullptr) {
    conf->endpoints_config = argv[1];
    return NGX_CONF_OK;
  }
  return "duplicate";
}

const char *esp_config_include(ngx_conf_t *cf, void *conf) {
  // Support for the Nginx config `include` directive.
  // http://nginx.org/en/docs/ngx_core_module.html#include
  return ngx_conf_include(cf, nullptr, nullptr);
}

ngx_str_t default_metadata_server = ngx_string("http://169.254.169.254");

const char *esp_set_metadata_server(ngx_conf_t *cf, ngx_esp_loc_conf_t *conf) {
  if (conf->metadata_server != NGX_CONF_UNSET) {
    return "duplicate";
  }

  ngx_str_t *argv = reinterpret_cast<ngx_str_t *>(cf->args->elts);
  if (cf->args->nelts == 1 ||
      ngx_string_equal(argv[1], ngx_string("default"))) {
    // "metadata_server" or "metadata_server default"
    conf->metadata_server = 1;
    conf->metadata_server_url = default_metadata_server;
  } else if (ngx_string_equal(argv[1], ngx_string("off"))) {
    // metadata_server off
    conf->metadata_server = 0;
  } else {
    // metadata_server <url>
    conf->metadata_server = 1;
    conf->metadata_server_url = argv[1];
  }

  return NGX_CONF_OK;
}

const char *esp_set_google_authentication_secret(ngx_conf_t *cf,
                                                 ngx_esp_loc_conf_t *conf) {
  if (conf->google_authentication_secret.data != nullptr) return "duplicate";
  ngx_str_t *argv = reinterpret_cast<ngx_str_t *>(cf->args->elts);
  ngx_str_t file_name = argv[1];
  ngx_str_t file_contents = ngx_null_string;
  if (ngx_conf_full_name(cf->cycle, &file_name, 1) == NGX_OK &&
      ngx_esp_read_file_null_terminate(reinterpret_cast<char *>(file_name.data),
                                       cf->pool, &file_contents) == NGX_OK) {
    conf->google_authentication_secret = file_contents;
    return NGX_CONF_OK;
  }
  ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "Failed to open file: %V",
                     &file_name);
  return reinterpret_cast<char *>(NGX_CONF_ERROR);
}

const char *esp_configure_service_control(ngx_conf_t *cf,
                                          ngx_esp_loc_conf_t *conf) {
  if (conf->service_control != NGX_CONF_UNSET) {
    return "duplicate";
  }

  ngx_str_t *argv = reinterpret_cast<ngx_str_t *>(cf->args->elts);
  if (ngx_string_equal(argv[1], ngx_string("off"))) {
    // service_control off
    conf->service_control = 0;
    // service_control default
  } else if (ngx_string_equal(argv[1], ngx_string("default"))) {
    conf->service_control = 1;
    ngx_str_null(&conf->service_controller_url);
  } else {
    // service_control <url>
    conf->service_control = 1;
    conf->service_controller_url = argv[1];
  }

  return NGX_CONF_OK;
}

const char *esp_configure_cloud_tracing(ngx_conf_t *cf,
                                        ngx_esp_loc_conf_t *conf) {
  if (conf->cloud_tracing != NGX_CONF_UNSET) {
    return "duplicate";
  }

  ngx_str_t *argv = reinterpret_cast<ngx_str_t *>(cf->args->elts);
  if (ngx_string_equal(argv[1], ngx_string("off"))) {
    // cloud_tracing off
    conf->cloud_tracing = 0;
  } else if (ngx_string_equal(argv[1], ngx_string("default"))) {
    // cloud_tracing default
    conf->cloud_tracing = 1;
    ngx_str_null(&conf->cloud_trace_api_url);
  } else {
    // cloud_tracing <url>
    conf->cloud_tracing = 1;
    conf->cloud_trace_api_url = argv[1];
  }

  return NGX_CONF_OK;
}

const char *esp_configure_api_authentication(ngx_conf_t *cf,
                                             ngx_esp_loc_conf_t *conf) {
  if (conf->api_authentication != NGX_CONF_UNSET) {
    return "duplicate";
  }

  ngx_str_t *argv = reinterpret_cast<ngx_str_t *>(cf->args->elts);
  if (ngx_string_equal(argv[1], ngx_string("off"))) {
    // api_authentication off
    conf->api_authentication = 0;
  } else if (ngx_string_equal(argv[1], ngx_string("default"))) {
    // api_authentication default
    conf->api_authentication = 1;
  } else {
    return "invalid";
  }

  return NGX_CONF_OK;
}

const char *esp_set_server_config(ngx_conf_t *cf, ngx_esp_loc_conf_t *conf) {
  if (conf->endpoints_server_config.data != nullptr) return "duplicate";
  ngx_str_t *argv = reinterpret_cast<ngx_str_t *>(cf->args->elts);
  ngx_str_t file_name = argv[1];
  ngx_str_t file_contents = ngx_null_string;
  if (ngx_conf_full_name(cf->cycle, &file_name, 1) == NGX_OK &&
      ngx_esp_read_file_null_terminate(reinterpret_cast<char *>(file_name.data),
                                       cf->pool, &file_contents) == NGX_OK) {
    conf->endpoints_server_config = file_contents;
    return NGX_CONF_OK;
  }
  ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "Failed to open file: %V",
                     &file_name);
  return reinterpret_cast<char *>(NGX_CONF_ERROR);
}

// Enum specifying to which level of ESP configuration a setting applies.
enum EspConfiguration {
  // Main configuration. A handler is passed ngx_esp_main_conf_t*.
  ESP_MAIN_CONF,
  // Server configuration. A handler is passed ngx_esp_srv_conf_t*.
  ESP_SRV_CONF,
  // Location configuration. A handler is passed ngx_esp_loc_conf_t*.
  ESP_LOC_CONF
};

// An ESP config parser handler type.
// Note that the configuration is expressed as void* but specific implementation
// use the level-specific configuration pointer type, one of:
// ngx_esp_(main|srv|loc)_conf_t*.
typedef const char *(*esp_conf_handler_pt)(ngx_conf_t *cf, void *conf);

// An ESP configuration command record.
// The config parser finds command by name and, if # of arguments matches, calls
// the setter with the appropriate configuration object.
typedef struct esp_command_s {
  // Config command name.
  const char *name;
  // Minimum and maximum # of arguments the command accepts.
  ngx_uint_t min_args : 4;
  ngx_uint_t max_args : 4;
  // Marks deprecated commands. A deprecated command is still handled by the
  // parser but will generate a warning.
  bool deprecated;
  // Type of ESP configuration object accepted by the setter.
  EspConfiguration conf;
  // The setter function pointer.
  esp_conf_handler_pt set;
} esp_command_t;

#define ESP_COMMAND(name, min, max, conf, set) \
  { name, min, max, false, conf, reinterpret_cast<esp_conf_handler_pt>(set) }

// Deprecated commands.
#define ESP_DCOMMAND(name, min, max, conf, set) \
  { name, min, max, true, conf, reinterpret_cast<esp_conf_handler_pt>(set) }

esp_command_t esp_commands[] = {
    ESP_COMMAND("on", 0, 0, ESP_LOC_CONF, esp_enable_endpoints),
    ESP_COMMAND("off", 0, 0, ESP_LOC_CONF, esp_disable_endpoints),
    ESP_COMMAND("api", 1, 1, ESP_LOC_CONF, esp_set_endpoints_config),
    ESP_COMMAND("include", 1, 1, ESP_LOC_CONF, esp_config_include),
    // When present, instructs ESP to use Google Compute Engine metadata server
    // to determine compute zone, and possibly other attributes, of the ESP
    // execution environment.
    // An optional argument can specify the metadata server URL. Defaults to
    // http://169.254.169.254.
    ESP_COMMAND("metadata_server", 0, 1, ESP_LOC_CONF, esp_set_metadata_server),
    // Client secret to generate tokens for authenticating against Google
    // services like service-control, cloud-trace API, etc.
    ESP_COMMAND("google_authentication_secret", 1, 1, ESP_LOC_CONF,
                esp_set_google_authentication_secret),
    // Old name for google_authentication_secret
    ESP_COMMAND("servicecontrol_secret", 1, 1, ESP_LOC_CONF,
                esp_set_google_authentication_secret),
    // Service-control override, usage:
    //   service_control off | default | <url>
    // 'off' - force disables the service-control feature,
    // 'default' - uses the configuration in service config,
    // '<url>' - specifies the override url for the service controller.
    ESP_COMMAND("service_control", 1, 1, ESP_LOC_CONF,
                esp_configure_service_control),
    // Cloud-tracing override, usage:
    //   cloud_tracing off | default | <url>
    // 'off' - force disables the cloud-tracing feature,
    // 'default' - uses the configuration in service config,
    // '<url>' - specifies the override url for the Cloud Trace API.
    ESP_COMMAND("cloud_tracing", 1, 1, ESP_LOC_CONF,
                esp_configure_cloud_tracing),
    // Auth override, usage:
    //   api_authentication off | default
    // 'off' - force disables the authentication,
    // 'default' - uses the configuration in service config,
    ESP_COMMAND("api_authentication", 1, 1, ESP_LOC_CONF,
                esp_configure_api_authentication),
    ESP_COMMAND("server_config", 1, 1, ESP_LOC_CONF, esp_set_server_config),
};

// Parses an individual directive supported in the `endpoints` block.
// Supported directives are (in the order of implementation below):
//
// on;
//   - Enable API management for the context.
// off;
//   - API Management is disabled in the context.
// api <file path>;
//   - Endpoints configuration file name
// include <file/pattern>;
//   - Nginx config include directive.
// metadata_server [off | default | <url>]
//   - 'off' - metadata server is not used
//   - 'default' - uses the default metadata server (http://169.254.169.254),
//   - '<url>' - specifies a custom metadata server.
// service_control off | default | <url>
//   - 'off' - force disables the service-control feature,
//   - 'default' - uses the configuration in service config,
//   - '<url>' - specifies the override url for the service controller.
// cloud_tracing off | default | <url>
//   - 'off' - force disables the cloud-tracing feature,
//   - 'default' - uses the configuration in service config,
//   - '<url>' - specifies the override url for the Cloud Trace API.
// api_authentication off | default
//   - 'off' - force disables the authentication,
//   - 'default' - uses the configuration in service config,
// google_authentication_secret <file path>;
//   - path to a file containing an auth secret for authenticating Google
//     services like Service Controller, Cloud Trace API.
//
// Following directives are supported but actively deprecated.
// servicecontrol_secret <file path>;
//   - Same as google_authentication_secret
char *ngx_http_endpoints_command(ngx_conf_t *cf, ngx_command_t *dummy,
                                 void *conf) {
  const char *result = "unrecognized";

  ngx_str_t *argv = reinterpret_cast<ngx_str_t *>(cf->args->elts);
  const int count = sizeof(esp_commands) / sizeof(esp_commands[0]);
  for (int i = 0; i < count; i++) {
    esp_command_t *cmd = &esp_commands[i];
    if (ngx_strcmp(cmd->name, argv[0].data) != 0) continue;

    if (cmd->deprecated) {
      ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                         "A configuration %V is deprecated.", argv);
    }

    ngx_uint_t argc = cf->args->nelts - 1;
    if (argc < cmd->min_args || argc > cmd->max_args) {
      if (cmd->min_args == cmd->max_args) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "value %V accepts %d arguments. Received %d", argv,
                           cmd->min_args, argc);
      } else {
        ngx_conf_log_error(
            NGX_LOG_EMERG, cf, 0,
            "value %V accepts between %d and %d arguments. Received %d", argv,
            cmd->min_args, cmd->max_args, argc);
      }
      return reinterpret_cast<char *>(NGX_CONF_ERROR);
    }

    void *conf = nullptr;
    switch (cmd->conf) {
      case ESP_MAIN_CONF:
        conf = ngx_http_conf_get_module_main_conf(cf, ngx_esp_module);
        break;
      case ESP_SRV_CONF:
        conf = ngx_http_conf_get_module_srv_conf(cf, ngx_esp_module);
        break;
      case ESP_LOC_CONF:
        conf = ngx_http_conf_get_module_loc_conf(cf, ngx_esp_module);
        break;
    }

    result = cmd->set(cf, conf);
    break;
  }

  if (result == NGX_CONF_OK || result == NGX_CONF_ERROR) {
    return const_cast<char *>(result);
  }

  ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "value %V is %s", argv, result);
  return reinterpret_cast<char *>(NGX_CONF_ERROR);
}

}  // namespace

// Parse the `endpoints` configuration block.
// An example `endpoints` block is:
//
// endpoints {
//   api <configuration file path>;
//   on|off;
// }
char *ngx_http_endpoints_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
  ngx_esp_loc_conf_t *lc = reinterpret_cast<ngx_esp_loc_conf_t *>(conf);

  if (lc->endpoints_block) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "duplicate endpoints block");
    return reinterpret_cast<char *>(NGX_CONF_ERROR);
  }
  lc->endpoints_block = 1;

  ngx_conf_t endpoints_cf = *cf;
  endpoints_cf.handler = ngx_http_endpoints_command;
  endpoints_cf.handler_conf = reinterpret_cast<char *>(conf);

  return ngx_conf_parse(&endpoints_cf, 0);
}

char *ngx_esp_configure_status_handler(ngx_conf_t *cf, ngx_command_t *cmd,
                                       void *conf) {
  ngx_int_t rc = ngx_esp_add_stats_shared_memory(cf);
  if (rc != NGX_OK) {
    return reinterpret_cast<char *>(NGX_CONF_ERROR);
  }

  auto *clcf = reinterpret_cast<ngx_http_core_loc_conf_t *>(
      ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module));

  clcf->handler = ngx_esp_status_handler;

  return NGX_CONF_OK;
}

ngx_int_t ngx_esp_read_file(const char *filename, ngx_pool_t *pool,
                            ngx_str_t *data) {
  return ngx_esp_read_file_impl(filename, pool, data, 0);
}

ngx_int_t ngx_esp_read_file_null_terminate(const char *filename,
                                           ngx_pool_t *pool, ngx_str_t *data) {
  return ngx_esp_read_file_impl(filename, pool, data, 1);
}

namespace {

// Parses a protobuf message from JSON. Returns true if successful; otherwise
// returns false.
bool ngx_esp_parse_message_from_json(const std::string &json,
                                     ::google::protobuf::Message *message) {
  static const char kTypeUrlPrefix[] = "type.googleapis.com";

  // Create a TypeResolver based on the protoc generated descriptor pool
  static ::google::protobuf::util::TypeResolver *type_resolver =
      ::google::protobuf::util::NewTypeResolverForDescriptorPool(
          kTypeUrlPrefix, ::google::protobuf::DescriptorPool::generated_pool());

  // The type url of the message type to be converted
  std::string type_url =
      std::string(kTypeUrlPrefix) + "/" + message->GetDescriptor()->full_name();

  // Try to convert to a binary message
  std::string binary;
  ::google::protobuf::util::Status status =
      ::google::protobuf::util::JsonToBinaryString(type_resolver, type_url,
                                                   json, &binary);
  if (!status.ok()) {
    return false;
  }

  // Now deserialize the binary message
  return message->ParseFromString(binary);
}

// Reads server config from a string. Returns true if successful, otherwise
// returns false.
// NOTE: This is similiar to ReadConfigFromString() in
//       src/api_manager/config.cc. We are repeating it here to avoid additional
//       dependencies. If there are more utilities like this to be duplicated,
//       we should think of a place for utilities that are common for
//       api_manager library and NGINX module.
bool ngx_esp_read_server_config_from_string(const std::string &str,
                                            ServerConfig *config) {
  // Try binary serialized proto first. Due to a bug in JSON parser,
  // JSON parser may crash if presented with non-JSON data.
  if (config->ParseFromString(str)) {
    return true;
  }

  // Now try JSON
  if (ngx_esp_parse_message_from_json(str, config)) {
    return true;
  }

  // Finally, try text format.
  return ::google::protobuf::TextFormat::ParseFromString(str, config);
}

}  // namespace

ngx_int_t ngx_esp_build_server_config(ngx_conf_t *cf, ngx_esp_loc_conf_t *lc,
                                      std::string *server_config) {
  // NOTE: We serialize the server config here and then deserialize it in the
  //       ApiManager layer for simplicity. If this turns out to slow-down ESP
  //       startup time (unlikely), we can pass the ServerConfig object instead
  //       of the string.

  // Parse the base server config if specified
  ServerConfig config;
  if (lc->endpoints_server_config.data != nullptr &&
      !ngx_esp_read_server_config_from_string(
          ngx_str_to_std(lc->endpoints_server_config), &config)) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "Failed to parse the server config.");
    return NGX_ERROR;
  }

  // Merge the values overwritten in nginx.conf

  // Metadata Server URL
  if (lc->metadata_server != NGX_CONF_UNSET) {
    config.mutable_metadata_server_config()->set_enabled(lc->metadata_server ==
                                                         1);
    config.mutable_metadata_server_config()->set_url(
        ngx_str_to_std(lc->metadata_server_url));
  }

  // Google Authentication Secret
  if (lc->google_authentication_secret.data != nullptr) {
    config.set_google_authentication_secret(
        ngx_str_to_std(lc->google_authentication_secret));
  }

  // Service Control
  if (lc->service_control != NGX_CONF_UNSET) {
    config.mutable_service_control_config()->set_force_disable(
        lc->service_control == 0);

    if (lc->service_control == 1 &&
        lc->service_controller_url.data != nullptr) {
      config.mutable_service_control_config()->set_url_override(
          ngx_str_to_std(lc->service_controller_url));
    }
  }

  // Cloud Tracing
  if (lc->cloud_tracing != NGX_CONF_UNSET) {
    config.mutable_cloud_tracing_config()->set_force_disable(
        lc->cloud_tracing == 0);

    if (lc->cloud_tracing == 1 && lc->cloud_trace_api_url.data != nullptr) {
      config.mutable_cloud_tracing_config()->set_url_override(
          ngx_str_to_std(lc->cloud_trace_api_url));
    }
  }

  // API Authentication
  if (lc->api_authentication != NGX_CONF_UNSET) {
    config.mutable_api_authentication_config()->set_force_disable(
        lc->api_authentication == 0);
  }

  // the config file from nginx config will override the ones from
  // server_config.
  if (lc->endpoints_config.len > 0) {
    auto service_config_rollout = config.mutable_service_config_rollout();
    auto traffic_percentages =
        service_config_rollout->mutable_traffic_percentages();
    traffic_percentages->clear();

    ngx_str_t file_name = lc->endpoints_config;
    if (ngx_conf_full_name(cf->cycle, &file_name, 1) != NGX_OK) {
      ngx_conf_log_error(
          NGX_LOG_EMERG, cf, 0,
          "Failed to resolve an api service configuration file: %V",
          &file_name);
      return NGX_ERROR;
    }

    (*traffic_percentages)[ngx_str_to_std(file_name)] = 100.0;
  }

  // Reserialize
  if (!config.SerializeToString(server_config)) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "Failed to serialize the server config.");
    return NGX_ERROR;
  }

  return NGX_OK;
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
