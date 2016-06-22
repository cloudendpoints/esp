# Copyright (C) Endpoints Server Proxy Authors
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
################################################################################
#
load("/perl", "perl_library")
load("/nginx", "nginx_test", "nginx_suite")

perl_library(
    name = "perl_library",
    srcs = glob(["*.pm"]),
    data = [
        "//include:version",
        "//src/tools:auth_token_gen",
        "//src/tools:service_control_json_gen",
    ] + glob(["testdata/*"]),
    deps = [
        "//third_party:nginx_test",
    ],
)

nginx_suite(
    data = [
        "matching-client-secret.json",
    ],
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "auth_check_report_body.t",
        "auth_ok_check_fail.t",
        "auth_pass_user_info.t",
        "auth_pkey_cache.t",
        "auth_unreachable_pkey.t",
        "new_http.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    data = [
        "matching-client-secret.json",
        "wrong-client-secret.json",
    ],
    nginx = "//src/nginx/main:nginx-esp",
    tags = ["exclusive"],
    tests = [
        "auth_pkey.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    data = [
        "matching-client-secret-symmetric.json",
        "wrong-client-secret-symmetric.json",
    ],
    nginx = "//src/nginx/main:nginx-esp",
    tags = ["exclusive"],
    tests = [
        "auth_symmetrickey.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    size = "small",
    data = [
        "matching-client-secret.json",
        "testdata/grpc_tsan_suppressions",
        "//test/grpc:grpc-test-client",
        "//test/grpc:grpc-test-server",
    ],
    env = {
        "TSAN_OPTIONS": "suppressions=$TEST_SRCDIR/src/nginx/t/testdata/grpc_tsan_suppressions",
    },
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "grpc_lib_big_streaming_report.t",
        "grpc_lib_downstream_flow_control.t",
        "grpc_lib_error_report.t",
        "grpc_lib_failed_check.t",
        "grpc_lib_forbidden_service_control.t",
        "grpc_lib_grpc.t",
        "grpc_lib_grpc_error_report.t",
        "grpc_lib_grpc_no_backend.t",
        "grpc_lib_simple_report.t",
        "grpc_lib_ssl_downstream.t",
        "grpc_lib_streaming_report.t",
        "grpc_lib_upstream_flow_control.t",
        "grpc_pass_api_key.t",
        "grpc_pass_auth_pkey.t",
        "grpc_pass_call_flow_control.t",
        "grpc_pass_config_addr.t",
        "grpc_pass_downstream_flow_control.t",
        "grpc_pass_errors.t",
        "grpc_pass_grpc_fallback.t",
        "grpc_pass_grpc_override.t",
        "grpc_pass_non_grpc.t",
        "grpc_pass_ssl_downstream.t",
        "grpc_pass_streaming.t",
        "grpc_pass_upstream_flow_control.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    size = "medium",
    data = [
        "testdata/grpc_tsan_suppressions",
        "//test/grpc:grpc-test-client",
        "//test/grpc:grpc-test-server",
    ],
    env = {
        "TSAN_OPTIONS": "suppressions=$TEST_SRCDIR/src/nginx/t/testdata/grpc_tsan_suppressions",
    },
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "grpc_lib_call_flow_control.t",
        "grpc_lib_ministress.t",
        "grpc_pass_ministress.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "aggregated_check_report.t",
        "auth_no_auth.t",
        "auth_no_pkey.t",
        "by_consumer_metrics.t",
        "check.t",
        "check_api_key.t",
        "check_custom_api_key.t",
        "check_default.t",
        "check_referer.t",
        "check_report_body.t",
        "check_with_token.t",
        "config_missing.t",
        "cors.t",
        "failed_check.t",
        "metadata.t",
        "metadata_fail.t",
        "metadata_timeout.t",
        "no_service_control.t",
        "report_3xx.t",
        "report_4xx.t",
        "report_compute_platform.t",
        "report_failed_request.t",
        "report_with_geo.t",
        "statistics.t",
        "test_all_http_methods.t",
        "unrecognized_method.t",
        "unregistered.t",
        "unregistered_no_project.t",
        "unspecified_service_control.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    config_list = [
        "//src/nginx/t:testdata/sc_timeout.pb.txt",
    ],
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "check_network_failures.t",
    ],
    deps = [
        ":perl_library",
    ],
)

# Tests in this suite don't use %%TEST_CONFIG%%. so it is not using
# config_list parameter.
nginx_suite(
    nginx = "//src/nginx/main:nginx-esp",
    tags = ["exclusive"],
    tests = [
        "auth_openid.t",
        "auth_ssl.t",
        "cloud_trace.t",
        "cloud_trace_disabled.t",
        "endpoints_off.t",
        "no_check.t",
        "no_http.t",
        "status.t",
    ],
    deps = [
        ":perl_library",
    ],
)

# Tests in this suite don't use %%TEST_CONFIG%%. so it is not using
# config_list parameter.
nginx_suite(
    data = [
        "testdata/grpc_tsan_suppressions",
        "//test/grpc:grpc-test-client",
        "//test/grpc:grpc-test-server",
    ],
    env = {
        "TSAN_OPTIONS": "suppressions=$TEST_SRCDIR/src/nginx/t/testdata/grpc_tsan_suppressions",
    },
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "grpc_lib_grpc_override.t",
        "grpc_lib_grpc_with_http.t",
    ],
    deps = [
        ":perl_library",
    ],
)

# This test suite contains the server config flavor for the new HTTP
# subrequest implementation. For now ESP only loads the config but
# does not yet put it in effect so we are only testing loading for now.
# Once the implementation comes online the new HTTP config will be used
# in the larger test suites.
nginx_suite(
    size = "small",
    config_list = [
        "//src/nginx/t:testdata/sc_override.pb.txt",
    ],
    nginx = "//src/nginx/main:nginx-esp",
    tags = ["exclusive"],
    tests = [
        "override_service_control.t",
    ],
    deps = [
        ":perl_library",
    ],
)
