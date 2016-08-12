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
load("@protobuf_git//:protobuf.bzl", "cc_proto_library")

cc_proto_library(
    name = "status_proto",
    srcs = [
        "proto/status.proto",
    ],
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    deps = [
        "//external:cc_wkt_protos",
        "//src/api_manager:status_proto",
    ],
)

cc_library(
    name = "ngx_esp",
    srcs = [
        "alloc.h",
        "config.cc",
        "config.h",
        "environment.cc",
        "environment.h",
        "error.cc",
        "error.h",
        "grpc.cc",
        "grpc.h",
        "grpc_finish.cc",
        "grpc_finish.h",
        "grpc_passthrough_server_call.cc",
        "grpc_passthrough_server_call.h",
        "grpc_queue.cc",
        "grpc_queue.h",
        "grpc_server_call.cc",
        "grpc_server_call.h",
        "http.cc",
        "http.h",
        "module.cc",
        "module.h",
        "request.cc",
        "request.h",
        "response.cc",
        "response.h",
        "status.cc",
        "status.h",
        "transcoded_grpc_server_call.cc",
        "transcoded_grpc_server_call.h",
        "util.cc",
        "util.h",
        "zero_copy_stream.cc",
        "zero_copy_stream.h",
    ],
    defines = [
        "NGX_HTTP_ENDPOINTS_RUNTIME",
    ],
    visibility = [":__subpackages__"],
    deps = [
        ":status_proto",
        "//external:grpc",
        "//external:grpc++",
        "//external:protobuf",
        "//include:api_manager",
        "//src/api_manager/grpc",
        "//src/api_manager/grpc:zero_copy_stream",
        "//third_party/config:servicecontrol",
        "//third_party/nginx:core",
        "//third_party/nginx:http",
    ],
)
