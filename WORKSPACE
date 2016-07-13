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

bind(
    name = "protobuf_compiler",
    actual = "//google/protobuf:protoc_lib",
)

bind(
    name = "protobuf_clib",
    actual = "//google/protobuf:protobuf_lite",
)

git_repository(
    name = "grpc_git",
    commit = "03efbd34ce64615f58007eae667b375accc6c8e6",  # 0.15.0
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
    name = "grpc++_reflection",
    actual = "@grpc_git//:grpc++_reflection",
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

git_repository(
    name = "boringssl_git",
    commit = "f7cc893d5032d11ae32646f93ace1c1237b9f463",  # 2016-07-07
    remote = "https://boringssl.googlesource.com/boringssl",
)

bind(
    name = "boringssl_crypto",
    actual = "@boringssl_git//:crypto",
)

bind(
    name = "boringssl_ssl",
    actual = "@boringssl_git//:ssl",
)

# Required by gRPC.
bind(
    name = "libssl",
    actual = "@boringssl_git//:ssl",
)

new_http_archive(
    name = "pcre_http",
    build_file = "third_party/BUILD.pcre",
    sha256 = "9883e419c336c63b0cb5202b09537c140966d585e4d0da66147dc513da13e629",
    strip_prefix = "pcre-8.38",
    type = "tar.gz",
    url = "https://storage.googleapis.com/build-dependencies/pcre-8.38.tar.gz",
)

bind(
    name = "pcre",
    actual = "@pcre_http//:pcre",
)

new_git_repository(
    name = "zlib_git",
    build_file = "third_party/BUILD.zlib",
    commit = "50893291621658f355bc5b4d450a8d06a563053d",
    remote = "https://github.com/madler/zlib.git",
)

bind(
    name = "zlib",
    actual = "@zlib_git//:zlib",
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
    name = "nginx_pkgoss_git",
    build_file = "third_party/BUILD.nginx-pkgoss",
    commit = "c5b295b180da34576d86f5371662c1a744f4cd2e",  # 2016-02-24
    remote = "https://nginx.googlesource.com/nginx-pkgoss",
)

bind(
    name = "nginx_config_includes",
    actual = "//third_party/nginx:config_includes",
)

bind(
    name = "nginx_html_files",
    actual = "//third_party/nginx:html_files",
)
