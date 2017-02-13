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

ISTIO_PROXY = "76355366e4adc6d4002d69d5aafa57e1606338ee"

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

# Required by gRPC.
bind(
    name = "libssl",
    actual = "@boringssl//:ssl",
)

git_repository(
    name = "istio_proxy_git",
    commit = ISTIO_PROXY,
    remote = "https://github.com/istio/proxy",
)

load(
    "@istio_proxy_git//contrib/endpoints:repositories.bzl",
    "grpc_repositories",
    "servicecontrol_client_repositories",
)
load(
    "@istio_proxy_git//:repositories.bzl",
    "protobuf_repositories",
    "googletest_repositories",
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

bind(
    name = "grpc_transcoding",
    actual = "@istio_proxy_git//contrib/endpoints/src/grpc/transcoding",
)

servicecontrol_client_repositories()

protobuf_repositories()

googletest_repositories()

grpc_repositories()

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
    name = "gflags_git",
    commit = "fe57e5af4db74ab298523f06d2c43aa895ba9f98",  # 2016-07-22
    remote = "https://github.com/gflags/gflags",
)

bind(
    name = "gflags",
    actual = "@gflags_git//:gflags",
)

git_repository(
    name = "tools",
    commit = "3327bae27498025ef8d33709f37182ae407fc517",
    remote = "https://github.com/cloudendpoints/endpoints-tools",
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
    commit = "76c63b5cd0d47c1f2b47ab4953db96c574af1c1d",
    remote = "https://github.com/bazelbuild/rules_go.git",
)

load("@io_bazel_rules_go//go:def.bzl", "go_repositories", "go_repository", "new_go_repository")

go_repositories()

new_git_repository(
    name = "github_com_golang_protobuf",
    build_file = "third_party/BUILD.golang_protobuf",
    commit = "8616e8ee5e20a1704615e6c8d7afcdac06087a67",
    remote = "https://github.com/golang/protobuf.git",
)

load("//test/grpc:repositories.bzl", "grpc_go_repositories")

grpc_go_repositories()
