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
# A Bazel (http://bazel.io) workspace for the Google Cloud Endpoints runtime.

load("//third_party/nginx:build.bzl", "nginx_repositories")

nginx_repositories(
    bind = True,
    nginx = "@//third_party/nginx",
)

# Required by gRPC.
bind(
    name = "libssl",
    actual = "@boringssl//:ssl",
)

# Though GRPC has BUILD file, our own BUILD.grpc file is needed since it contains
# more targets including testing server and client.
# To generate the BUILD.grpc file, cherry-pick
# https://github.com/grpc/grpc/pull/7556
# and run ./tools/buildgen/generate_projects.sh in GRPC repo.
new_git_repository(
    name = "grpc_git",
    commit = "2a69139aa7f609e439c24a46754252a5f9d37500",  # v1.0.0
    remote = "https://github.com/grpc/grpc.git",
    build_file = "third_party/BUILD.grpc",
)

bind(
    name = "gpr",
    actual = "@grpc_git//:gpr",
)

bind(
    name = "grpc",
    actual = "@grpc_git//:grpc",
)

bind(
    name = "grpc_cpp_plugin",
    actual = "@grpc_git//:grpc_cpp_plugin",
)

bind(
    name = "grpc++",
    actual = "@grpc_git//:grpc++",
)

bind(
    name = "grpc_lib",
    actual = "@grpc_git//:grpc++_reflection",
)

new_git_repository(
    name = "protobuf_git",
    commit = "e8ae137c96444ea313485ed1118c5e43b2099cf1",  # v3.0.0
    remote = "https://github.com/google/protobuf.git",
    build_file = "third_party/BUILD.protobuf",
)

bind(
    name = "protoc",
    actual = "@protobuf_git//:protoc",
)

bind(
    name = "protobuf",
    actual = "@protobuf_git//:protobuf",
)

bind(
    name = "cc_wkt_protos",
    actual = "@protobuf_git//:cc_wkt_protos",
)

bind(
    name = "cc_wkt_protos_genproto",
    actual = "@protobuf_git//:cc_wkt_protos_genproto",
)

bind(
    name = "protobuf_compiler",
    actual = "@protobuf_git//:protoc_lib",
)

bind(
    name = "protobuf_clib",
    actual = "@protobuf_git//:protobuf_lite",
)

new_git_repository(
    name = "nanopb_git",
    build_file = "third_party/BUILD.nanopb",
    commit = "f8ac463766281625ad710900479130c7fcb4d63b",
    remote = "https://github.com/nanopb/nanopb.git",
)

bind(
    name = "nanopb",
    actual = "@nanopb_git//:nanopb",
)

new_git_repository(
    name = "googletest_git",
    build_file = "third_party/BUILD.googletest",
    commit = "d225acc90bc3a8c420a9bcd1f033033c1ccd7fe0",
    remote = "https://github.com/google/googletest.git",
)

bind(
    name = "googletest",
    actual = "@googletest_git//:googletest",
)

bind(
    name = "googletest_main",
    actual = "@googletest_git//:googletest_main",
)

bind(
    name = "googletest_prod",
    actual = "@googletest_git//:googletest_prod",
)

new_git_repository(
    name = "googleapis_git",
    commit = "2608c0a7a988c62ac1c5f38c7f1f0516430ad1de",
    remote = "https://github.com/googleapis/googleapis.git",
    build_file = "third_party/BUILD.googleapis",
)

bind(
    name = "servicecontrol",
    actual = "@googleapis_git//:servicecontrol",
)

bind(
    name = "servicecontrol_genproto",
    actual = "@googleapis_git//:servicecontrol_genproto",
)

bind(
    name = "service_config",
    actual = "@googleapis_git//:service_config",
)

bind(
    name = "cloud_trace",
    actual = "@googleapis_git//:cloud_trace",
)

git_repository(
    name = "gflags_git",
    commit = "fe57e5af4db74ab298523f06d2c43aa895ba9f98", # 2016-07-22
    remote = "https://github.com/gflags/gflags",
)

bind(
    name = "gflags",
    actual = "@gflags_git//:gflags",
)

#
# Go dependencies
#

git_repository(
    name = "io_bazel_rules_perl",
    remote = "https://github.com/bazelbuild/rules_perl.git",
    commit = "b081f8e57b6fb0dbb3991687abef00a9a7529343",
)

git_repository(
    name = "io_bazel_rules_go",
    remote = "https://github.com/bazelbuild/rules_go.git",
    tag = "0.1.0",
)
load("@io_bazel_rules_go//go:def.bzl", "go_repositories")
go_repositories()

new_git_repository(
    name = "github_com_golang_protobuf",
    remote = "https://github.com/golang/protobuf.git",
    commit = "c3cefd437628a0b7d31b34fe44b3a7a540e98527",
    build_file = "third_party/BUILD.golang_protobuf",
)

new_git_repository(
    name = "github_com_spf13_pflag",
    remote = "https://github.com/spf13/pflag.git",
    commit = "6fd2ff4ff8dfcdf5556fbdc0ac0284408274b1a7",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/spf13")
go_library(
    name = "pflag",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = [
        "//visibility:public",
    ]
)
    """
)

new_git_repository(
    name = "github_com_spf13_cobra",
    remote = "https://github.com/spf13/cobra.git",
    commit = "9c28e4bbd74e5c3ed7aacbc552b2cab7cfdfe744",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/spf13")
go_library(
    name = "cobra",
    srcs = [
        "bash_completions.go",
        "cobra.go",
        "command.go",
        "command_notwin.go",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@github_com_spf13_pflag//:pflag",
    ],
)
    """
)

new_git_repository(
    name = "github_com_golang_glog",
    remote = "https://github.com/golang/glog.git",
    commit = "23def4e6c14b4da8ac2ed8007337bc5eb5007998",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/golang")
go_library(
    name = "glog",
    srcs = ["glog.go", "glog_file.go"],
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "github_com_google_gofuzz",
    remote = "https://github.com/google/gofuzz",
    commit = "bbcb9da2d746f8bdbd6a936686a0a6067ada0ec5",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/google")
go_library(
    name = "gofuzz",
    srcs = ["fuzz.go", "doc.go"],
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "go_spew",
    remote = "https://github.com/davecgh/go-spew",
    commit = "5215b55f46b2b919f50a1df0eaa5886afe4e3b3d",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/davecgh/go-spew")
go_library(
    name = "spew",
    srcs = glob(include = ["spew/*.go"], exclude = ["spew/*_test.go", "spew/bypasssafe.go"]),
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "pborman_uuid",
    remote = "https://github.com/pborman/uuid",
    commit = "ca53cad383cad2479bbba7f7a1a05797ec1386e4",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/pborman")
go_library(
    name = "uuid",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "go_codec",
    remote = "https://github.com/ugorji/go.git",
    commit = "f1f1a805ed361a0e078bb537e4ea78cd37dcf065",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/ugorji/go")
go_library(
    name = "codec",
    srcs = glob(include = ["codec/*.go"],
      exclude = ["codec/*_test.go",
                 "codec/helper_not_unsafe.go",
                 "codec/fast-path.not.go"]),
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "go_yaml",
    remote = "https://github.com/go-yaml/yaml.git",
    commit = "e4d366fc3c7938e2958e662b4258c7a89e1f0e3e",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("gopkg.in")
go_library(
    name = "yaml.v2",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "ghodss_yaml",
    remote = "https://github.com/ghodss/yaml",
    commit = "73d445a93680fa1a78ae23a5839bad48f32ba1ee",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/ghodss")
go_library(
    name = "yaml",
    srcs = ["yaml.go", "fields.go"],
    visibility = ["//visibility:public"],
    deps = ["@go_yaml//:yaml.v2"],
)
    """
)

new_git_repository(
    name = "github_com_gogo_protobuf",
    remote = "https://github.com/gogo/protobuf.git",
    commit = "e18d7aa8f8c624c915db340349aad4c49b10d173",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/gogo/protobuf")
go_library(
    name = "proto",
    srcs = glob(include = ["proto/*.go"], exclude = ["proto/*_test.go", "proto/pointer_reflect.go"]),
    visibility = ["//visibility:public"],
)
go_library(
    name = "sortkeys",
    srcs = ["sortkeys/sortkeys.go"],
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "github_com_x_net",
    remote = "https://github.com/golang/net.git",
    commit = "e90d6d0afc4c315a0d87a568ae68577cc15149a0",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("golang.org/x/net")
go_library(
    name = "context",
    srcs = ["context/context.go", "context/go17.go"],
    visibility = ["//visibility:public"],
)
go_library(
    name = "context/ctxhttp",
    srcs = ["context/ctxhttp/ctxhttp.go"],
    visibility = ["//visibility:public"],
    deps = ["context"],
)
go_library(
    name = "lex/httplex",
    srcs = ["lex/httplex/httplex.go"],
    visibility = ["//visibility:public"],
)
go_library(
    name = "http2/hpack",
    srcs = glob(include = ["http2/hpack/*.go"], exclude = ["http2/hpack/*_test.go"]),
    visibility = ["//visibility:public"],
)
go_library(
    name = "http2",
    srcs = glob(include = ["http2/*.go"], exclude = ["http2/*_test.go", "http2/go17.go", "http2/not_go16.go"]),
    visibility = ["//visibility:public"],
    deps = [
        ":http2/hpack",
        ":lex/httplex",
    ],
)
    """
)

new_git_repository(
    name = "docker",
    remote = "https://github.com/docker/distribution.git",
    commit = "cd27f179f2c10c5d300e6d09025b538c475b0d51",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/docker/distribution")
go_library(
    name = "reference",
    srcs = ["reference/reference.go", "reference/regexp.go"],
    visibility = ["//visibility:public"],
    deps = [":digest"],
)
go_library(
    name = "digest",
    srcs = glob(include=["digest/*.go"],exclude=["digest/*_test.go"]),
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "blang_semver",
    remote = "https://github.com/blang/semver.git",
    commit = "31b736133b98f26d5e078ec9eb591666edfd091f",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/blang")
go_library(
    name = "semver",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "juju_ratelimit",
    remote = "https://github.com/juju/ratelimit.git",
    commit = "77ed1c8a01217656d2080ad51981f6e99adaa177",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/juju")
go_library(
    name = "ratelimit",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "go_inf",
    remote = "https://github.com/go-inf/inf.git",
    tag = "v0.9.0",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("gopkg.in")
go_library(
    name = "inf.v0",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "imdario_mergo",
    remote = "https://github.com/imdario/mergo.git",
    commit = "6633656539c1639d9d78127b7d47c622b5d7b6dc",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/imdario")
go_library(
    name = "mergo",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "go_restful",
    remote = "https://github.com/emicklei/go-restful.git",
    commit = "bf50d2be18145391aa3d4339b07195807b25a427",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/emicklei")
go_library(
    name = "go-restful",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [":go-restful/log"],
)
go_library(
    name = "go-restful/swagger",
    srcs = glob(include = ["swagger/*.go"], exclude = ["swagger/*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [":go-restful", ":go-restful/log"],
)
go_library(
    name = "go-restful/log", srcs = ["log/log.go"],
    visibility = ["//visibility:public"],
)
    """
)

new_git_repository(
    name = "github_com_kubernetes_client_go",
    remote = "https://github.com/kubernetes/client-go.git",
    commit = "e1a6aa664414135c03f74bac140f30323bb49ce1",
    build_file = "third_party/BUILD.kubernetes",
)

new_git_repository(
    name = "google_api_go_client",
    remote = "https://github.com/google/google-api-go-client.git",
    commit = "a69f0f19d246419bb931b0ac8f4f8d3f3e6d4feb",
    build_file = "third_party/BUILD.google_api_go_client",
)

new_git_repository(
    name = "golang_oauth2",
    remote = "https://github.com/golang/oauth2.git",
    commit = "3c3a985cb79f52a3190fbc056984415ca6763d",
    build_file = "third_party/BUILD.golang_oauth2",
)

new_git_repository(
    name = "google_cloud_go",
    remote = "https://github.com/GoogleCloudPlatform/google-cloud-go.git",
    commit = "290ed46a0684cc372f475ca8f36b63aa3066978e",
    build_file = "third_party/BUILD.google_cloud_go",
)
