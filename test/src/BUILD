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
load(
    "@io_bazel_rules_go//go:def.bzl",
    "go_prefix",
    "go_library",
    "go_binary",
    "go_test",
)

# Define an import prefix
go_prefix("github.com/cloudendpoints/esp")

# NOTE: Below we use a special "vendor/" keyword in target names to specify
# custom import paths for go libraries. Otherwise, go_library() rule
# automatically generates an import path using the package name and the target
# name. We do this for two reasones:
#   1) to leave intact the generated *.pb.go code (not to change the import
#      paths)
#   2) to be able to locally develop using native go tools( i.e. "go get" and
#      "go run") by setting the GOPATH to the <esp_root>/test directory.
#      Notice that all go libraries in this file have the
#      "vendor/<path_from_test_src>" form.

go_library(
    name = "vendor/google/protobuf",
    srcs = glob(
        ["google/protobuf/*.pb.go"],
    ),
    go_prefix = "//test/src:go_prefix",
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
    go_prefix = "//test/src:go_prefix",
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
    go_prefix = "//test/src:go_prefix",
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
    go_prefix = "//test/src:go_prefix",
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
    go_prefix = "//test/src:go_prefix",
    deps = [
        "@github_com_golang_protobuf//:proto",
    ],
)

go_library(
    name = "vendor/google/api/servicecontrol/v1",
    srcs = glob(
        ["google/api/servicecontrol/v1/*.pb.go"],
    ),
    go_prefix = "//test/src:go_prefix",
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
    go_prefix = "//test/src:go_prefix",
    deps = [
        "//test/src:vendor/google/api/servicecontrol/v1",
        "@github_com_golang_protobuf//:proto",
    ],
)

go_library(
    name = "vendor/deploy",
    srcs = [
        "deploy/service.go",
        "deploy/default_service_config.go",
    ],
    go_prefix = "//test/src:go_prefix",
    deps = [
        ":vendor/google/api",
        "@golang_oauth2//:oauth2/google",
        "@golang_oauth2//:oauth2/jwt",
        "@github_com_x_net//:context",
        "@google_api_go_client//:servicemanagement/v1",
        "@github_com_golang_protobuf//:jsonpb",
        "@github_com_golang_protobuf//:proto",
        "@go_yaml//:yaml.v2",
    ],
)

go_library(
    name = "vendor/cli",
    srcs = glob([
        "cli/*.go",
    ]),
    go_prefix = "//test/src:go_prefix",
    deps = [
        ":vendor/utils",
        ":vendor/deploy",
        "@google_api_go_client//:logging/v2beta1",
        "@golang_oauth2//:oauth2/google",
        "@github_com_x_net//:context",
        "@github_com_spf13_cobra//:cobra",
        "@github_com_kubernetes_client_go//:kubernetes",
        "@github_com_kubernetes_client_go//:pkg/labels",
        "@github_com_kubernetes_client_go//:tools/clientcmd",
        "@github_com_kubernetes_client_go//:pkg/api",
        "@github_com_kubernetes_client_go//:pkg/api/v1",
        "@github_com_kubernetes_client_go//:pkg/util/intstr",
        "@github_com_kubernetes_client_go//:pkg/apis/extensions/v1beta1",
    ],
)

go_library(
    name = "vendor/controller",
    srcs = [
        "controller/annotations.go",
        "controller/config.go",
        "controller/config_store.go",
        "controller/controller.go",
        "controller/nginx.go",
        "controller/queue.go",
        "controller/utils.go",
    ],
    go_prefix = "//test/src:go_prefix",
    deps = [
        ":vendor/deploy",
        ":vendor/utils",
        "@github_com_kubernetes_client_go//:kubernetes",
        "@github_com_kubernetes_client_go//:tools/cache",
        "@github_com_kubernetes_client_go//:pkg/api",
        "@github_com_kubernetes_client_go//:pkg/api/v1",
        "@github_com_kubernetes_client_go//:pkg/apis/extensions/v1beta1",
        "@github_com_kubernetes_client_go//:pkg/watch",
        "@github_com_kubernetes_client_go//:pkg/runtime",
        "@github_com_kubernetes_client_go//:pkg/util/flowcontrol",
        "@github_com_kubernetes_client_go//:pkg/util/intstr",
        "@github_com_golang_glog//:glog",
    ],
    data = [
        "controller/nginx.tmpl",
    ],
)

go_binary(
    name = "espcli",
    srcs = [
        "espcli.go",
    ],
    go_prefix = "//test/src:go_prefix",
    deps = [
        ":vendor/cli",
        "@github_com_spf13_cobra//:cobra",
    ],
)

go_binary(
    name = "service_config",
    srcs = [
        "service_config.go",
    ],
    go_prefix = "//test/src:go_prefix",
    deps = [
        ":vendor/deploy",
        "@google_api_go_client//:servicemanagement/v1",
        "@github_com_spf13_cobra//:cobra",
    ],
)

go_binary(
    name = "ingress",
    srcs = [
        "ingress.go",
    ],
    data = [
        "//src/nginx/main:nginx-esp",
    ],
    go_prefix = "//test/src:go_prefix",
    deps = [
        ":vendor/controller",
        "@github_com_kubernetes_client_go//:kubernetes",
        "@github_com_kubernetes_client_go//:pkg/api",
        "@github_com_kubernetes_client_go//:rest",
        "@github_com_kubernetes_client_go//:tools/clientcmd",
        "@github_com_golang_glog//:glog",
    ],
)


go_binary(
    name = "fakes_server",
    srcs = [
        "fakes/server/main.go",
    ],
    go_prefix = "//test/src:go_prefix",
    deps = [
        ":vendor/fakes",
    ],
)

go_library(
    name = "vendor/deploy/deploy-local",
    srcs = [
        "deploy/deploy-local/deploy-local.go",
    ],
    go_prefix = "//test/src:go_prefix",
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
        "//include:version",
        "//src/nginx/t:testdata/logs_metrics.pb.txt",
        "//src/tools:auth_token_gen",
        "utils/nginx.crt",
        "utils/nginx.key",
        "utils/credentials.json",
    ],
    go_prefix = "//test/src:go_prefix",
    deps = [
        ":vendor/google/api/servicecontrol/v1",
        ":vendor/google/logging/type",
        "@github_com_golang_protobuf//:proto",
        "@github_com_golang_protobuf//:ptypes/struct",
    ],
)

go_test(
    name = "config_test",
    tags = ["exclusive"],
    srcs = ["controller/config_test.go"],
    data = [
        "//src/nginx/main:nginx-esp",
    ],
    go_prefix = "//test/src:go_prefix",
    deps = [
        ":vendor/controller",
        ":vendor/utils",
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
    go_prefix = "//test/src:go_prefix",
    deps = [
        ":vendor/deploy/deploy-local",
        ":vendor/fakes",
        ":vendor/utils",
    ],
)
