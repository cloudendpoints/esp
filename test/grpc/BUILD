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

package(default_visibility = [
    "//src/nginx/t:__pkg__",
])

load("@protobuf_bzl//:protobuf.bzl", "cc_proto_library")

cc_proto_library(
    name = "grpc-test_proto",
    srcs = ["grpc-test.proto"],
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    use_grpc_plugin = True,
    deps = [
        "//external:servicecontrol",
    ],
)

genrule(
    name = "grpc-test_descriptor",
    srcs = [
        "@googleapis_git//:google/rpc/status.proto",
        "@googleapis_git//:google/api/servicecontrol/v1/service_controller.proto",
        "@googleapis_git//:google/api/servicecontrol/v1/check_error.proto",
        "@googleapis_git//:google/api/servicecontrol/v1/distribution.proto",
        "@googleapis_git//:google/api/servicecontrol/v1/log_entry.proto",
        "@googleapis_git//:google/api/servicecontrol/v1/metric_value.proto",
        "@googleapis_git//:google/api/servicecontrol/v1/operation.proto",
        "@googleapis_git//:google/api/annotations.proto",
        "@googleapis_git//:google/api/http.proto",
        "@googleapis_git//:google/logging/type/log_severity.proto",
        "@googleapis_git//:google/type/money.proto",
        "@protobuf_git//:well_known_protos",
        "grpc-test.proto",
    ],
    outs = ["grpc-test.descriptor"],
    cmd = """$(location //external:protoc) $(location :grpc-test.proto) \
        -I$$(dirname $(location @googleapis_git//:google/rpc/status.proto))/../.. \
        -Iexternal/protobuf_git/src \
        -I$$(dirname $(location :grpc-test.proto)) \
        --include_imports --descriptor_set_out=$@""",
    tools = ["//external:protoc"],
)

genrule(
    name = "grpc-interop_descriptor",
    srcs = [
        "@protobuf_git//:well_known_protos",
        "@org_golang_google_grpc//interop/grpc_testing:go_default_library_protos",
    ],
    outs = ["grpc-interop.descriptor"],
    cmd = """$(location //external:protoc) \
        $(location @org_golang_google_grpc//interop/grpc_testing:go_default_library_protos) \
        -I$$(dirname $(location @org_golang_google_grpc//interop/grpc_testing:go_default_library_protos)) \
        -Iexternal/protobuf_git/src \
        --include_imports --descriptor_set_out=$@""",
    tools = ["//external:protoc"],
)

cc_library(
    name = "client-test-lib",
    testonly = 1,
    srcs = ["client-test-lib.cc"],
    hdrs = ["client-test-lib.h"],
    linkopts = ["-lm"],
    visibility = ["//visibility:private"],
    deps = [
        ":grpc-test_proto",
        "//external:api_manager_utils",
        "//external:grpc++",
    ],
)

cc_binary(
    name = "grpc-test-client",
    testonly = 1,
    srcs = ["grpc-test-client.cc"],
    deps = [
        ":client-test-lib",
        ":grpc-test_proto",
        "//external:api_manager_utils",
    ],
)

cc_binary(
    name = "grpc-perf-graphs",
    testonly = 1,
    srcs = ["grpc-perf-graphs.cc"],
    deps = [
        ":client-test-lib",
        ":grpc-test_proto",
    ],
)

cc_binary(
    name = "grpc-test-server",
    testonly = 1,
    srcs = ["grpc-test-server.cc"],
    deps = [
        ":grpc-test_proto",
        "//external:grpc++",
    ],
)

load(
    "@io_bazel_rules_go//go:def.bzl",
    "go_prefix",
    "go_binary",
)

go_prefix("github.com/cloudendpoints/esp/test/grpc")

go_binary(
    name = "interop-client",
    srcs = [
        "interop-client.go",
    ],
    deps = [
        "@org_golang_x_net//context:go_default_library",
        "@org_golang_google_grpc//:go_default_library",
        "@org_golang_google_grpc//credentials:go_default_library",
        "@org_golang_google_grpc//credentials/oauth:go_default_library",
        "@org_golang_google_grpc//grpclog:go_default_library",
        "@org_golang_google_grpc//interop:go_default_library",
        "@org_golang_google_grpc//interop/grpc_testing:go_default_library",
    ],
    go_prefix = ":go_prefix",
)


# Export service.json such that the tests can use it
exports_files(["local/service.json"])
