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

git_repository(
    name = "nginx",
    commit = "f5bf2d17902d1b504faac1a266883dab29dbff75",  # nginx-1.15.6
    remote = "https://nginx.googlesource.com/nginx",
)

load("@nginx//:build.bzl", "nginx_repositories")

nginx_repositories(
    bind = True,
    nginx = "@nginx//",
)

# Needs to come after nginx
git_repository(
    name = "iap_jwt_verify_nginx",
    commit = "cbdfb7aa74897230c23a46162e3fbe0209cb659a",  # Jan 8, 2018
    remote = "https://github.com/GoogleCloudPlatform/appengine-sidecars-docker",
)

load("@iap_jwt_verify_nginx//:iap_jwt_verify_nginx.bzl", "iap_jwt_verify_nginx_repositories")

iap_jwt_verify_nginx_repositories(True)

git_repository(
    name = "com_github_grpc_grpc",
    commit = "d2c7d4dea492b9a86a53555aabdbfa90c2b01730",  # v1.15.0
    remote = "https://github.com/grpc/grpc.git",
)
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps", "grpc_test_only_deps")
grpc_deps()
grpc_test_only_deps()

bind(
    name = "gpr",
    actual = "@com_github_grpc_grpc//:gpr",
)
bind(
    name = "grpc",
    actual = "@com_github_grpc_grpc//:grpc",
)
bind(
    name = "grpc_cpp_plugin",
    actual = "@com_github_grpc_grpc//:grpc_cpp_plugin",
)
bind(
    name = "grpc++",
    actual = "@com_github_grpc_grpc//:grpc++",
)
bind(
    name = "grpc_lib",
    actual = "@com_github_grpc_grpc//:grpc++_codegen_proto",
)

load(
    "//:repositories.bzl",
    "googletest_repositories",
    #"grpc_repositories",
    "protobuf_repositories",
    "servicecontrol_client_repositories",
    "transcoding_repositories",
)

bind(
    name = "api_manager",
    actual = "//include:api_manager",
)

bind(
    name = "api_manager_status_proto",
    actual = "//src/api_manager:status_proto",
)

bind(
    name = "api_manager_status_proto_genproto",
    actual = "//src/api_manager:status_proto_genproto",
)

bind(
    name = "api_manager_auth_lib",
    actual = "//src/api_manager/auth/lib",
)

bind(
    name = "api_manager_utils",
    actual = "//src/api_manager/utils",
)

bind(
    name = "grpc_transcoding",
    actual = "//src/grpc/transcoding:transcoding_endpoints",
)

servicecontrol_client_repositories()

protobuf_repositories()

googletest_repositories()

transcoding_repositories()

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
    commit = "0c5773db01ab8aeb3ae749b2fc570749b93af41f",
    remote = "https://github.com/benley/bazel_rules_pex.git",
)

load("@io_bazel_rules_pex//pex:pex_rules.bzl", "pex_repositories")

pex_repositories()

#
# Perl rules
#

git_repository(
    name = "io_bazel_rules_perl",
    commit = "48c7edfa7e130e35f894eb7249eb885428a213e1",
    remote = "https://github.com/bazelbuild/rules_perl.git",
)

#
# Go rules
#
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# download go bazel tools
http_archive(
    name = "io_bazel_rules_go",
    urls = ["https://github.com/bazelbuild/rules_go/releases/download/0.16.0/rules_go-0.16.0.tar.gz"],
    sha256 = "ee5fe78fe417c685ecb77a0a725dc9f6040ae5beb44a0ba4ddb55453aad23a8a",
)

# download the gazelle tool
http_archive(
    name = "bazel_gazelle",
    urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/0.15.0/bazel-gazelle-0.15.0.tar.gz"],
    sha256 = "6e875ab4b6bf64a38c352887760f21203ab054676d9c1b274963907e0768740d",
)

# load go rules
load("@io_bazel_rules_go//go:def.bzl", "go_rules_dependencies", "go_register_toolchains")
go_rules_dependencies()
go_register_toolchains()

# load gazelle
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")
gazelle_dependencies()

#
# Go repositories
#

go_repository(
    name = "org_golang_google_grpc",
    commit = "9bf8ea0a8282ebecd1aa474c926e3028f5c22a4c",
    importpath = "google.golang.org/grpc",
)

go_repository(
    name = "com_github_golang_protobuf",
    commit = "d7fc20193620986259ffb1f9b9da752114ee14a4",
#    commit = "3a3da3a4e26776cc22a79ef46d5d58477532dede",  # master, as of 2018-05-22
    importpath = "github.com/golang/protobuf",
    build_file_proto_mode = "disable",
)

go_repository(
    name = "org_golang_google_genproto",
    commit = "bb3573be0c484136831138976d444b8754777aff",
    importpath = "google.golang.org/genproto",
)

go_repository(
    name = "org_golang_x_net",
    commit = "513929065c19401a1c7b76ecd942f9f86a0c061b",
    importpath = "golang.org/x/net",
)

go_repository(
    name = "org_golang_x_oauth2",
    commit = "f047394b6d14284165300fd82dad67edb3a4d7f6",
    importpath = "golang.org/x/oauth2",
)

go_repository(
    name = "com_google_cloud_go",
    commit = "0625e1e4bfc1aa7a07d6285541fa9020feab1013",
    importpath = "cloud.google.com/go",
)

go_repository(
    name = "com_github_googleapis_gax_go",
    commit = "9af46dd5a1713e8b5cd71106287eba3cefdde50b",
    importpath = "github.com/googleapis/gax-go",
)

go_repository(
    name = "org_golang_x_text",
    commit = "19e51611da83d6be54ddafce4a4af510cb3e9ea4",
    importpath = "golang.org/x/text",
)
