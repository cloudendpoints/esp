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

load("@protobuf_bzl//:protobuf.bzl", "cc_proto_library")

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
    ],
)

cc_binary(
    name = "bookstore-client",
    testonly = 1,
    srcs = ["bookstore-client.cc"],
    linkopts = ["-lm"],
    deps = [
        ":bookstore",
        "//external:protobuf",
        "//external:gflags",
        "//external:grpc++",
    ],
)

# Export service.pb.txt such that the tests can use it
exports_files(["service.pb.txt"])
