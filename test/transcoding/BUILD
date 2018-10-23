load("@io_bazel_rules_go//go:def.bzl", "go_library")
load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")

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

load("@protobuf_git//:protobuf.bzl", "cc_proto_library")

cc_proto_library(
    name = "bookstore",
    srcs = ["bookstore.proto"],
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    use_grpc_plugin = True,
    deps = [
        "//external:cc_wkt_protos",
    ],
)

cc_binary(
    name = "bookstore-server",
    testonly = 1,
    srcs = ["bookstore-server.cc"],
    deps = [
        ":bookstore",
        "//external:grpc++",
        "//external:servicecontrol",
    ],
)

cc_binary(
    name = "bookstore-client",
    testonly = 1,
    srcs = ["bookstore-client.cc"],
    linkopts = ["-lm"],
    deps = [
        ":bookstore",
        "//external:gflags",
        "//external:grpc++",
        "//external:protobuf",
    ],
)

# Export service.pb.txt such that the tests can use it
exports_files(["service.pb.txt"])

proto_library(
    name = "endpoints_examples_bookstore_proto",
    srcs = ["bookstore.proto"],
    deps = [
        "@com_google_protobuf//:empty_proto",
        "@com_google_protobuf//:struct_proto",
    ],
)

go_proto_library(
    name = "endpoints_examples_bookstore_go_proto",
    compilers = ["@io_bazel_rules_go//proto:go_grpc"],
    importpath = "test/transcoding",
    proto = ":endpoints_examples_bookstore_proto",
)

go_library(
    name = "go_default_library",
    embed = [":endpoints_examples_bookstore_go_proto"],
    importpath = "test/transcoding",
)
