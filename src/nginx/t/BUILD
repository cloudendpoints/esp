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
################################################################################
#
load("@io_bazel_rules_perl//perl:perl.bzl", "perl_library")
load("/nginx", "nginx_test", "nginx_suite")

perl_library(
    name = "perl_library",
    srcs = glob(["*.pm"]),
    data = [
        "//src/nginx:version",
        "//src/tools:auth_token_gen",
        "//src/tools:service_control_json_gen",
    ] + glob(["testdata/*"]),
    deps = [
        "//third_party:nginx_test",
    ],
)

# Tests are allocated 10 ports consecutively in this
# BUILD file

nginx_suite(
    data = [
        "matching-client-secret.json",
    ],
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "auth_asymmetrickey.t",
        "auth_check_report_body.t",
        "auth_ok_check_fail.t",
        "auth_pass_user_info.t",
        "auth_pkey_cache.t",
        "auth_redirect.t",
        "auth_remove_user_info.t",
        "auth_unreachable_pkey.t",
        "new_http.t",
        "service_control_disabled.t",
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
        "//test/grpc:grpc-test-client",
        "//test/grpc:grpc-test-server",
    ],
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "grpc_api_key.t",
        "grpc_auth_pkey.t",
        "grpc_call_flow_control.t",
        "grpc_cloud_trace.t",
        "grpc_compression.t",
        "grpc_config_addr.t",
        "grpc_errors.t",
        "grpc_grpc_fallback.t",
        "grpc_grpc_override.t",
        "grpc_large_request.t",
        "grpc_large_response.t",
        "grpc_large_streaming.t",
        "grpc_long_streaming.t",
        "grpc_metadata.t",
        "grpc_reject_no_backend.t",
        "grpc_reject_non_grpc.t",
        "grpc_shared_port_ssl.t",
        "grpc_ssl_downstream.t",
        "grpc_streaming.t",
        "grpc_uds.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    size = "small",
    data = [
        "//test/grpc:interop-client",
        "@org_golang_google_grpc//interop/server",
    ],
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "grpc_interop_cancel.t",
        "grpc_interop_metadata.t",
        "grpc_interop_status.t",
        "grpc_interop_streaming.t",
        "grpc_interop_unary.t",
        "grpc_web_interop_empty.t",
        "grpc_web_interop_status.t",
        "grpc_web_interop_unary.t",
        "grpc_web_interop_unary_large.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    size = "medium",
    data = [
        "@org_golang_google_grpc//interop/server",
        "@org_golang_google_grpc//stress/client",
    ],
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        # grpc_interop_ministress is disabled as it is a flaky test
        #"grpc_interop_ministress.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    size = "medium",
    data = [
        "//test/grpc:grpc-test-client",
        "//test/grpc:grpc-test-server",
    ],
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "grpc_ministress.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "aggregated_check_report.t",
        "api_rewrite.t",
        "auth_disabled.t",
        "auth_no_auth.t",
        "auth_no_pkey.t",
        "backend_routing_constant_address.t",
        "backend_routing_append_path.t",
        "by_consumer_metrics.t",
        "check.t",
        "check_api_key.t",
        "check_custom_api_key.t",
        "check_default.t",
        "check_http_method_override.t",
        "check_invalid_api_key.t",
        "check_key_restriction.t",
        "check_no_consumer.t",
        "check_referer.t",
        "check_report_body.t",
        "check_report_metrics.t",
        "check_with_token.t",
        "config_extra_field.t",
        "config_missing.t",
        "config_rollouts_managed.t",
        "cors.t",
        "cors_disabled.t",
        "fail_wrong_api_key.t",
        "failed_check.t",
        "init_service_configs_multiple.t",
        "init_service_configs_single.t",
        "metadata.t",
        "metadata_fail.t",
        "metadata_timeout.t",
        "metadata_token.t",
        "multiple_apis.t",
        "no_backend.t",
        "no_service_control.t",
        "quota.t",
        "quota_api_not_available.t",
        "quota_exhausted.t",
        "reject_unrecognized.t",
        "report_3xx.t",
        "report_4xx.t",
        "report_with_headers.t",
        "report_compute_platform.t",
        "report_failed.t",
        "report_failed_request.t",
        "report_with_geo.t",
        "skip_service_control.t",
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
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "check_network_failures.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "cloud_trace.t",
        "cloud_trace_count_trigger.t",
        "cloud_trace_disabled.t",
        "cloud_trace_force_disabled.t",
        "cloud_trace_sampling.t",
        "cloud_trace_unknown_method.t",
        "endpoints_off.t",
        "no_check.t",
        "no_http.t",
        "status.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    nginx = "//src/nginx/main:nginx-esp",
    tags = ["exclusive"],
    tests = [
        "auth_openid.t",
        "auth_ssl.t",
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
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "override_service_control.t",
    ],
    deps = [
        ":perl_library",
    ],
)

nginx_suite(
    size = "small",
    data = [
        "matching-client-secret.json",
        "//test/grpc:grpc-test-server",
        "//test/grpc:local/service.json",
        "//test/transcoding:bookstore-client",
        "//test/transcoding:bookstore-server",
        "//test/transcoding:service.pb.txt",
    ],
    nginx = "//src/nginx/main:nginx-esp",
    tests = [
        "transcoding.t",
        "transcoding_auth.t",
        "transcoding_bindings.t",
        "transcoding_deep_struct.t",
        "transcoding_errors.t",
        "transcoding_h2.t",
        "transcoding_head.t",
        "transcoding_ignore_unknown_fields.t",
        "grpc_skip_host_header_in_metadata.t",
        "transcoding_large.t",
        "transcoding_metadata.t",
        "transcoding_query_params.t",
        #Temporarily disable the transcoding_shared_port_ssl.t test,
        #which occasionally fails in the Jenkins presubmit tests.
        #To-do: enable the transcoding_shared_port_ssl.t test after
        #the Jenkins problem is resolved.
        #"transcoding_shared_port_ssl.t",
        "transcoding_status.t",
        "transcoding_streaming.t",
        "transcoding_utf8.t",
    ],
    deps = [
        ":perl_library",
    ],
)

# Used by Go tests
exports_files(["matching-client-secret.json"])

exports_files(["testdata/logs_metrics.pb.txt"])
