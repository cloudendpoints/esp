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
load(
    "@io_bazel_rules_go//go:def.bzl",
    "go_library",
    "go_binary",
    "go_test",
)

# NOTE: Below we use a special "vendor/" keyword in target names to specify
# custom import paths for go libraries. Otherwise, go_library() rule
# automatically generates an import path using the package name and the target
# name. We do this for two reasones:
#   1) to leave intact the generated *.pb.go code (not to change the import
#      paths)
#   2) to be able to locally develop using native go tools( i.e. "go get" and
#      "go run") by setting the GOPATH to the <esp_root>/tools directory.
#      Notice that all go libraries in this file have the
#      "vendor/<path_from_src>" form.

go_library(
    name = "vendor/google/protobuf",
    srcs = glob(
        ["google/protobuf/*.pb.go"],
    ),
    importpath = "github.com/cloudendpoints/esp",
    deps = [
        "@github_com_golang_protobuf//:proto",
        "@github_com_golang_protobuf//:ptypes/any",
    ],
)

go_library(
    name = "vendor/google/api",
    srcs = glob(
        ["google/api/*.pb.go"],
    ),
    importpath = "github.com/cloudendpoints/esp",
    deps = [
        ":vendor/google/protobuf",
        "@github_com_golang_protobuf//:proto",
        "@github_com_golang_protobuf//:ptypes/any",
        "@github_com_golang_protobuf//:ptypes/timestamp",
        "@github_com_golang_protobuf//:ptypes/wrappers",
    ],
)

go_library(
    name = "vendor/google/rpc",
    srcs = glob(
        ["google/rpc/*.pb.go"],
    ),
    importpath = "github.com/cloudendpoints/esp",
    deps = [
        ":vendor/google/protobuf",
        "@github_com_golang_protobuf//:proto",
        "@github_com_golang_protobuf//:ptypes/any",
        "@github_com_golang_protobuf//:ptypes/duration",
    ],
)

go_library(
    name = "vendor/google/type",
    srcs = glob(
        ["google/type/*.pb.go"],
    ),
    importpath = "github.com/cloudendpoints/esp",
    deps = [
        ":vendor/google/protobuf",
        "@github_com_golang_protobuf//:proto",
    ],
)

go_library(
    name = "vendor/google/logging/type",
    srcs = glob(
        ["google/logging/type/*.pb.go"],
    ),
    importpath = "github.com/cloudendpoints/esp",
    deps = [
        "@github_com_golang_protobuf//:proto",
    ],
)

go_library(
    name = "vendor/google/api/servicecontrol/v1",
    srcs = glob(
        ["google/api/servicecontrol/v1/*.pb.go"],
    ),
    importpath = "github.com/cloudendpoints/esp",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":vendor/google/api",
        ":vendor/google/logging/type",
        ":vendor/google/protobuf",
        ":vendor/google/rpc",
        ":vendor/google/type",
        "@github_com_golang_protobuf//:jsonpb",
        "@github_com_golang_protobuf//:proto",
        "@github_com_golang_protobuf//:ptypes/any",
        "@github_com_golang_protobuf//:ptypes/struct",
        "@github_com_golang_protobuf//:ptypes/timestamp",
    ],
)

go_library(
    name = "vendor/fakes",
    srcs = [
        "fakes/config.go",
        "fakes/controller.go",
        "fakes/request_data.go",
        "fakes/request_parser.go",
    ],
    importpath = "github.com/cloudendpoints/esp",
    deps = [
        "//tools/src:vendor/google/api/servicecontrol/v1",
        "@github_com_golang_protobuf//:proto",
    ],
)

go_binary(
    name = "fakes_server",
    srcs = [
        "fakes/server/main.go",
    ],
    importpath = "github.com/cloudendpoints/esp",
    deps = [
        ":vendor/fakes",
    ],
)

go_library(
    name = "vendor/deploy/deploy-local",
    srcs = [
        "deploy/deploy-local/deploy-local.go",
    ],
    importpath = "github.com/cloudendpoints/esp",
)

go_library(
    name = "vendor/utils",
    srcs = [
        "utils/auth_token_gen.go",
        "utils/command.go",
        "utils/nginx_config.go",
        "utils/service_config.go",
        "utils/service_control.go",
        "utils/status.go",
        "utils/test_env_path.go",
    ],
    data = [
        "//src/nginx:version",
        "//src/nginx/t:testdata/logs_metrics.pb.txt",
        "//src/tools:auth_token_gen",
    ],
    importpath = "github.com/cloudendpoints/esp",
    deps = [
        ":vendor/google/api/servicecontrol/v1",
        ":vendor/google/logging/type",
        "@github_com_golang_protobuf//:proto",
        "@github_com_golang_protobuf//:ptypes/struct",
    ],
)

go_test(
    name = "t_test",
    srcs = [
        "t/backend.go",
        "t/esp.go",
        "t/metadata_server.go",
        "t/pk_server.go",
        "t/service_control.go",
        "t/setup.go",
        "t/t_test.go",
    ],
    data = [
        ":fakes_server",
        "//src/nginx/main:nginx-esp",
        "//src/nginx/t:matching-client-secret.json",
        "//src/tools:auth_token_gen",
    ],
    importpath = "github.com/cloudendpoints/esp",
    deps = [
        ":vendor/deploy/deploy-local",
        ":vendor/fakes",
        ":vendor/utils",
    ],
)
