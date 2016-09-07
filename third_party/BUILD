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
load("@io_bazel_rules_perl//perl:perl.bzl", "perl_library")
load("/nginx", "nginx_suite")
load("@protobuf_git//:protobuf.bzl", "cc_proto_library")

perl_library(
    name = "nginx_test",
    srcs = glob([
        "nginx-tests/lib/Test/**/*.pm",
    ]),
    visibility = ["//visibility:public"],
)

# Nginx test suite containing all of the integration tests
# included with Nginx.

# ssl_sni_reneg.t is excluded from the test suite because boring ssl
# does not support renegotiation feature required by the test.

nginx_suite(
    nginx = "//src/nginx/main:nginx-esp",
    tags = ["exclusive"],
    tests = glob(
        ["nginx-tests/*.t"],
        exclude = ["nginx-tests/ssl_sni_reneg.t"],
    ),
    deps = [
        ":nginx_test",
    ],
)

cc_proto_library(
    name = "service_config",
    srcs = [
        "googleapis/google/api/annotations.proto",
        "googleapis/google/api/auth.proto",
        "googleapis/google/api/backend.proto",
        "googleapis/google/api/billing.proto",
        "googleapis/google/api/consumer.proto",
        "googleapis/google/api/context.proto",
        "googleapis/google/api/control.proto",
        "googleapis/google/api/documentation.proto",
        "googleapis/google/api/http.proto",
        "googleapis/google/api/label.proto",
        "googleapis/google/api/log.proto",
        "googleapis/google/api/logging.proto",
        "googleapis/google/api/metric.proto",
        "googleapis/google/api/monitored_resource.proto",
        "googleapis/google/api/monitoring.proto",
        "googleapis/google/api/service.proto",
        "googleapis/google/api/system_parameter.proto",
        "googleapis/google/api/usage.proto",
    ],
    include = "googleapis",
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"],
    deps = [
        "//external:cc_wkt_protos",
    ],
)

cc_proto_library(
    name = "cloud_trace",
    srcs = [
        "googleapis/google/devtools/cloudtrace/v1/trace.proto",
    ],
    include = "googleapis",
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"],
    deps = [
        ":service_config",
        "//external:cc_wkt_protos",
    ],
)
