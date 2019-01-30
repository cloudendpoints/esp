# Copyright (C) Extensible Service Proxy Authors
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
###############################################################################
#

load("@io_bazel_rules_pex//pex:pex_rules.bzl", "pex_binary")

genrule(
    name = "import_nginx_conf_template",
    srcs = ["//start_esp:nginx-auto.conf.template"],
    outs = ["nginx-conf-template"],
    cmd = "cat $(location //start_esp:nginx-auto.conf.template) > $@",
)

genrule(
    name = "import_server_conf_template",
    srcs = ["//start_esp:server-auto.conf.template"],
    outs = ["server-conf-template"],
    cmd = "cat $(location //start_esp:server-auto.conf.template) > $@",
)

py_binary(
    name = "start_esp_binary",
    srcs = [
        "//start_esp:fetch_service_config.py",
        "//start_esp:start_esp.py",
    ],
    main = "//start_esp:start_esp.py",
)

py_test(
    name = "start_esp_test",
    srcs = [
        "start_esp_test.py",
    ],
    data = [
        "testdata/expected_access_log_nginx.conf",
        "testdata/expected_allow_invalid_headers_nginx.conf",
        "testdata/expected_backend_host_header_nginx.conf",
        "testdata/expected_backend_nginx.conf",
        "testdata/expected_backend_routing_nginx.conf",
        "testdata/expected_basic_nginx.conf",
        "testdata/expected_client_body_buffer_size_nginx.conf",
        "testdata/expected_client_ip_header_server.json",
        "testdata/expected_client_ip_position_server.json",
        "testdata/expected_client_max_body_size_nginx.conf",
        "testdata/expected_cloud_trace_url_override_server.json",
        "testdata/expected_cors_allow_credentials_nginx.conf",
        "testdata/expected_cors_allow_headers_nginx.conf",
        "testdata/expected_cors_allow_methods_nginx.conf",
        "testdata/expected_cors_allow_origin_nginx.conf",
        "testdata/expected_cors_allow_origin_regex_nginx.conf",
        "testdata/expected_cors_expose_headers_nginx.conf",
        "testdata/expected_cors_preset_basic_nginx.conf",
        "testdata/expected_disable_cloud_trace_auto_sampling_server.json",
        "testdata/expected_dns_nginx.conf",
        "testdata/expected_enable_debug_nginx.conf",
        "testdata/expected_enable_strict_transport_security_nginx.conf",
        "testdata/expected_enable_websocket_nginx.conf",
        "testdata/expected_experimental_enable_multiple_api_configs_disabled_nginx.conf",
        "testdata/expected_healthz_nginx.conf",
        "testdata/expected_management_server.json",
        "testdata/expected_metadata_nginx.conf",
        "testdata/expected_rewrite_server.json",
        "testdata/expected_rollout_strategy_server.json",
        "testdata/expected_service_control_url_override_server.json",
        "testdata/expected_service_control_report_http_headers.json",
        "testdata/expected_ssl_port_nginx.conf",
        "testdata/expected_ssl_protocols_nginx.conf",
        "testdata/expected_status_port_nginx.conf",
        "testdata/expected_transcoding_always_print_primitive_fields_server.json",
        "testdata/expected_underscores_in_headers_nginx.conf",
        "testdata/expected_worker_processes_nginx.conf",
        "testdata/test_service_config_1.json",
        ":nginx-conf-template",
        ":server-conf-template",
        ":start_esp_binary",
    ],
)
