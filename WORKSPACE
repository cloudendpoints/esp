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

load(
    "@bazel_tools//tools/build_defs/repo:git.bzl",
    "git_repository",
    "new_git_repository",
)
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Version from Jul 11, 2018 before they added a dependency on cc_common from
# a newer bazel version.
http_archive(
    name = "com_google_absl",
    strip_prefix = "abseil-cpp-37d45c0164671963051320598ee8421b87506283",
    urls = ["https://github.com/abseil/abseil-cpp/archive/37d45c0164671963051320598ee8421b87506283.zip"],
)

git_repository(
    name = "nginx",
    commit = "2746d4169f5376bb24c119e45aabe4fb70ca7150",  # nginx-1.15.9
    remote = "https://nginx.googlesource.com/nginx",
)

load("@nginx//:build.bzl", "nginx_repositories")

nginx_repositories(
    bind = True,
    nginx = "@nginx//",
    ngx_brotli = True,
)

# Needs to come after nginx
git_repository(
    name = "appengine_nginx",
    commit = "da931aa1a2a28b2e05f616889f998072e30aabee",  # Aug 11, 2020
    remote = "https://github.com/GoogleCloudPlatform/appengine-sidecars-docker",
)

load("@appengine_nginx//:appengine_nginx.bzl", "appengine_nginx_repositories")

appengine_nginx_repositories(True)

git_repository(
    name = "com_github_grpc_grpc",
    commit = "5110b5702daf7f27d31b0a54d118168d4b468edf",  # May 29, 2019
    remote = "https://github.com/grpc/grpc.git",
)

git_repository(
    name = "io_bazel_rules_python",
    commit = "8b5d0683a7d878b28fffe464779c8a53659fc645",
    remote = "https://github.com/bazelbuild/rules_python.git",
)

load("@io_bazel_rules_python//python:pip.bzl", "pip_repositories", "pip_import")

pip_import(
    name = "grpc_python_dependencies",
    requirements = "@com_github_grpc_grpc//:requirements.bazel.txt",
)

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps", "grpc_test_only_deps")

grpc_deps()

grpc_test_only_deps()

load("@com_github_grpc_grpc//bazel:grpc_python_deps.bzl", "grpc_python_deps")

grpc_python_deps()


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

bind(
    name = "absl_base_endian",
    actual = "@com_google_absl//absl/base:endian",
)

bind(
    name = "absl_strings",
    actual = "@com_google_absl//absl/strings",
)

load(
    "//:repositories.bzl",
    "googletest_repositories",
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
    commit = "0c5773db01ab8aeb3ae749b2fc570749b93af41f", # Jul 22, 2018
    remote = "https://github.com/benley/bazel_rules_pex.git",
)

load("@io_bazel_rules_pex//pex:pex_rules.bzl", "pex_repositories")

pex_repositories()

#
# Perl rules
#

git_repository(
    name = "io_bazel_rules_perl",
    commit = "5510c0ee04152aed9c5d1aba3ddb01daab336ad5",
    remote = "https://github.com/bazelbuild/rules_perl.git",
)

#
# Go rules
#
git_repository(
    name = "io_bazel_rules_go",
    commit = "2d792dea8d22c552f455623bb15eb4f61fcb2f1b",  # Dec 15, 2018 (v0.16.5)
    remote = "https://github.com/bazelbuild/rules_go.git",
)

http_archive(
    name = "bazel_gazelle",
    urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/0.16.0/bazel-gazelle-0.16.0.tar.gz"],
    sha256 = "7949fc6cc17b5b191103e97481cf8889217263acf52e00b560683413af204fcb",  # Jan 9, 2019 (v0.16.0)
)

load("@io_bazel_rules_go//go:def.bzl", "go_rules_dependencies", "go_register_toolchains")
go_rules_dependencies()
go_register_toolchains()
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
gazelle_dependencies()

new_git_repository(
    name = "github_com_golang_protobuf",
    build_file = "third_party/BUILD.golang_protobuf",
    commit = "8616e8ee5e20a1704615e6c8d7afcdac06087a67",
    remote = "https://github.com/golang/protobuf.git",
)
