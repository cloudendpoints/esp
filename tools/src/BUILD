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
#      "go run") by setting the GOPATH to the <esp_root>/tools directory.
#      Notice that all go libraries in this file have the
#      "vendor/<path_from_src>" form.

go_library(
    name = "vendor/google/protobuf",
    srcs = glob(
        ["google/protobuf/*.pb.go"],
    ),
    go_prefix = "//tools/src:go_prefix",
    deps = [
        "@com_github_golang_protobuf//proto:go_default_library",
        "@com_github_golang_protobuf//ptypes/any:go_default_library",
    ],
)

go_library(
    name = "vendor/google/api",
    srcs = glob(
        ["google/api/*.pb.go"],
    ),
    go_prefix = "//tools/src:go_prefix",
    deps = [
        ":vendor/google/protobuf",
        "@com_github_golang_protobuf//proto:go_default_library",
        "@com_github_golang_protobuf//ptypes/any:go_default_library",
        "@com_github_golang_protobuf//ptypes/timestamp:go_default_library",
        "@com_github_golang_protobuf//ptypes/wrappers:go_default_library",
    ],
)

go_library(
    name = "vendor/google/rpc",
    srcs = glob(
        ["google/rpc/*.pb.go"],
    ),
    go_prefix = "//tools/src:go_prefix",
    deps = [
        ":vendor/google/protobuf",
        "@com_github_golang_protobuf//proto:go_default_library",
        "@com_github_golang_protobuf//ptypes/any:go_default_library",
        "@com_github_golang_protobuf//ptypes/duration:go_default_library",
    ],
)

go_library(
    name = "vendor/google/type",
    srcs = glob(
        ["google/type/*.pb.go"],
    ),
    go_prefix = "//tools/src:go_prefix",
    deps = [
        ":vendor/google/protobuf",
        "@com_github_golang_protobuf//proto:go_default_library",
    ],
)

go_library(
    name = "vendor/google/logging/type",
    srcs = glob(
        ["google/logging/type/*.pb.go"],
    ),
    go_prefix = "//tools/src:go_prefix",
    deps = [
        "@com_github_golang_protobuf//proto:go_default_library",
    ],
)

go_library(
    name = "vendor/google/api/servicecontrol/v1",
    srcs = glob(
        ["google/api/servicecontrol/v1/*.pb.go"],
    ),
    go_prefix = "//tools/src:go_prefix",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":vendor/google/api",
        ":vendor/google/logging/type",
        ":vendor/google/protobuf",
        ":vendor/google/rpc",
        ":vendor/google/type",
        "@com_github_golang_protobuf//jsonpb:go_default_library",
        "@com_github_golang_protobuf//proto:go_default_library",
        "@com_github_golang_protobuf//ptypes/any:go_default_library",
        "@com_github_golang_protobuf//ptypes/struct:go_default_library",
        "@com_github_golang_protobuf//ptypes/timestamp:go_default_library",
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
    go_prefix = "//tools/src:go_prefix",
    deps = [
        "//tools/src:vendor/google/api/servicecontrol/v1",
        "@com_github_golang_protobuf//proto:go_default_library",
    ],
)


genrule(
    name = "vendor/version.go",
    srcs = ["//include:version"],
    outs = ["version/version.go"],
    cmd = """
    cat <<EOF > $@
package version
const Version = "$$(cat $(location //include:version))"
EOF
    """,
)

go_library(
    name = "vendor/version",
    srcs = ["version/version.go"],
    go_prefix="//tools/src:go_prefix",
)

go_library(
    name = "vendor/deploy",
    srcs = [
        "deploy/service.go",
        "deploy/default_service_config.go",
    ],
    go_prefix = "//tools/src:go_prefix",
    deps = [
        ":vendor/google/api",
        "@org_golang_x_oauth2//google:go_default_library",
        "@org_golang_x_oauth2//jwt:go_default_library",
        "@org_golang_x_net//context:go_default_library",
        "@org_golang_google_api//servicemanagement/v1:go_default_library",
        "@com_github_golang_protobuf//jsonpb:go_default_library",
        "@com_github_golang_protobuf//proto:go_default_library",
        "@in_gopkg_yaml_v2//:go_default_library",
    ],
)

go_library(
    name = "vendor/cli",
    srcs = glob([
        "cli/*.go",
    ]),
    go_prefix = "//tools/src:go_prefix",
    deps = [
        ":vendor/deploy",
        ":vendor/utils",
        ":vendor/version",
        "@org_golang_google_api//logging/v2beta1:go_default_library",
        "@org_golang_x_oauth2//google:go_default_library",
        "@org_golang_x_net//context:go_default_library",
        "@com_github_spf13_cobra//:go_default_library",
        "@io_k8s_client_go//1.5/kubernetes:go_default_library",
        "@io_k8s_client_go//1.5/pkg/labels:go_default_library",
        "@io_k8s_client_go//1.5/tools/clientcmd:go_default_library",
        "@io_k8s_client_go//1.5/pkg/api:go_default_library",
        "@io_k8s_client_go//1.5/pkg/api/v1:go_default_library",
        "@io_k8s_client_go//1.5/pkg/util/intstr:go_default_library",
        "@io_k8s_client_go//1.5/pkg/apis/extensions/v1beta1:go_default_library",
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
    go_prefix = "//tools/src:go_prefix",
    deps = [
        ":vendor/deploy",
        ":vendor/utils",
        "@io_k8s_client_go//1.5/kubernetes:go_default_library",
        "@io_k8s_client_go//1.5/tools/cache:go_default_library",
        "@io_k8s_client_go//1.5/pkg/api:go_default_library",
        "@io_k8s_client_go//1.5/pkg/api/v1:go_default_library",
        "@io_k8s_client_go//1.5/pkg/apis/extensions/v1beta1:go_default_library",
        "@io_k8s_client_go//1.5/pkg/watch:go_default_library",
        "@io_k8s_client_go//1.5/pkg/runtime:go_default_library",
        "@io_k8s_client_go//1.5/pkg/util/flowcontrol:go_default_library",
        "@io_k8s_client_go//1.5/pkg/util/intstr:go_default_library",
        "@com_github_golang_glog//:go_default_library",
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
    go_prefix = "//tools/src:go_prefix",
    deps = [
        ":vendor/cli",
        ":vendor/version",
        "@com_github_spf13_cobra//:go_default_library",
    ],
)

go_binary(
    name = "service_config",
    srcs = [
        "service_config.go",
    ],
    go_prefix = "//tools/src:go_prefix",
    deps = [
        ":vendor/deploy",
        "@org_golang_google_api//servicemanagement/v1:go_default_library",
        "@com_github_spf13_cobra//:go_default_library",
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
    go_prefix = "//tools/src:go_prefix",
    deps = [
        ":vendor/controller",
        "@io_k8s_client_go//1.5/kubernetes:go_default_library",
        "@io_k8s_client_go//1.5/pkg/api:go_default_library",
        "@io_k8s_client_go//1.5/rest:go_default_library",
        "@io_k8s_client_go//1.5/tools/clientcmd:go_default_library",
        "@com_github_golang_glog//:go_default_library",
    ],
)


go_binary(
    name = "fakes_server",
    srcs = [
        "fakes/server/main.go",
    ],
    go_prefix = "//tools/src:go_prefix",
    deps = [
        ":vendor/fakes",
    ],
)

go_library(
    name = "vendor/deploy/deploy-local",
    srcs = [
        "deploy/deploy-local/deploy-local.go",
    ],
    go_prefix = "//tools/src:go_prefix",
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
    ],
    go_prefix = "//tools/src:go_prefix",
    deps = [
        ":vendor/google/api/servicecontrol/v1",
        ":vendor/google/logging/type",
        "@com_github_golang_protobuf//proto:go_default_library",
        "@com_github_golang_protobuf//ptypes/struct:go_default_library",
    ],
)

go_test(
    name = "config_test",
    tags = ["exclusive"],
    srcs = ["controller/config_test.go"],
    data = [
        "//src/nginx/main:nginx-esp",
        ":controller/nginx.tmpl",
        ":testdata/credentials.json",
        ":testdata/nginx.key",
        ":testdata/nginx.crt",
    ],
    go_prefix = "//tools/src:go_prefix",
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
    go_prefix = "//tools/src:go_prefix",
    deps = [
        ":vendor/deploy/deploy-local",
        ":vendor/fakes",
        ":vendor/utils",
    ],
)
