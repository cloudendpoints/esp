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
# A Bazel (http://bazel.io) workspace for the Google Cloud Endpoints runtime.

ISTIO_PROXY = "db51059faa238bd7af39b754834ecc6acd11cca0"

git_repository(
    name = "nginx",
    commit = "cfce863dbe786e61e0e7a33376906b337da70c19",
    remote = "https://nginx.googlesource.com/nginx",
)

load("@nginx//:build.bzl", "nginx_repositories")

nginx_repositories(
    bind = True,
    nginx = "@nginx//",
)

git_repository(
    name = "istio_proxy_git",
    commit = ISTIO_PROXY,
    remote = "https://github.com/istio/proxy",
)

bind(
    name = "api_manager",
    actual = "@istio_proxy_git//contrib/endpoints/include:api_manager",
)

bind(
    name = "api_manager_status_proto",
    actual = "@istio_proxy_git//contrib/endpoints/src/api_manager:status_proto",
)

bind(
    name = "api_manager_status_proto_genproto",
    actual = "@istio_proxy_git//contrib/endpoints/src/api_manager:status_proto_genproto",
)

bind(
    name = "api_manager_auth_lib",
    actual = "@istio_proxy_git//contrib/endpoints/src/api_manager/auth/lib",
)

bind(
    name = "api_manager_utils",
    actual = "@istio_proxy_git//contrib/endpoints/src/api_manager/utils",
)

git_repository(
    name = "servicecontrol_client_git",
    commit = "d739d755365c6a13d0b4164506fd593f53932f5d",
    remote = "https://github.com/cloudendpoints/service-control-client-cxx.git",
)

bind(
    name = "servicecontrol_client",
    actual = "@servicecontrol_client_git//:service_control_client_lib",
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
    build_file = "third_party/BUILD.grpc",
    commit = "d28417c856366df704200f544e72d31056931bce",
    init_submodules = True,
    remote = "https://github.com/grpc/grpc.git",
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

# Workaround for Bazel > 0.4.0 since it needs newer protobuf.bzl from:
# https://github.com/google/protobuf/pull/2246
# Do not use this git_repository for anything else than protobuf.bzl
new_git_repository(
    name = "protobuf_bzl",
    # Injecting an empty BUILD file to prevent using any build target
    build_file_content = "",
    commit = "05090726144b6e632c50f47720ff51049bfcbef6",
    remote = "https://github.com/google/protobuf.git",
)

git_repository(
    name = "protobuf_git",
    commit = "a428e42072765993ff674fda72863c9f1aa2d268",  # v3.1.0
    remote = "https://github.com/google/protobuf.git",
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
    build_file = "third_party/BUILD.googleapis",
    commit = "db1d4547dc56a798915e0eb2c795585385922165",
    remote = "https://github.com/googleapis/googleapis.git",
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
    commit = "fe57e5af4db74ab298523f06d2c43aa895ba9f98",  # 2016-07-22
    remote = "https://github.com/gflags/gflags",
)

bind(
    name = "gflags",
    actual = "@gflags_git//:gflags",
)

#
# Python rules
#

git_repository(
    name = "io_bazel_rules_pex",
    commit = "d4af3ca0a015e8b2d2a81a4df1df51bb0fa0bba0",
    remote = "https://github.com/benley/bazel_rules_pex.git",
)

load("@io_bazel_rules_pex//pex:pex_rules.bzl", "pex_repositories")

pex_repositories()

#
# Perl rules
#

git_repository(
    name = "io_bazel_rules_perl",
    commit = "f6211c2db1e54d0a30bc3c3a718f2b5d45f02a22",
    remote = "https://github.com/bazelbuild/rules_perl.git",
)

#
# Go rules
#
git_repository(
    name = "io_bazel_rules_go",
    commit = "3b13b2dba81e09ec213ccbd4da56ad332cb5d3dc",
    remote = "https://github.com/bazelbuild/rules_go.git",
)

load("@io_bazel_rules_go//go:def.bzl", "go_repositories", "go_repository")

go_repositories()

new_git_repository(
    name = "github_com_golang_protobuf",
    build_file = "third_party/BUILD.golang_protobuf",
    commit = "8616e8ee5e20a1704615e6c8d7afcdac06087a67",
    remote = "https://github.com/golang/protobuf.git",
)
