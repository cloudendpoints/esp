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
cc_library(
    name = "grpc",
    srcs = [
        "proxy_flow.cc",
        "proxy_flow.h",
        "stub_map.cc",
        "stub_map.h",
    ],
    hdrs = [
        "server_call.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//external:grpc",
        "//external:grpc++",
        "//external:protobuf",
        "//external:servicecontrol",
        "//src/api_manager",
    ],
)

cc_library(
    name = "zero_copy_stream",
    srcs = [
        "message_serializer.cc",
        "zero_copy_stream.cc",
    ],
    hdrs = [
        "message_serializer.h",
        "zero_copy_stream.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//external:grpc",
        "//external:grpc++",
        "//external:protobuf",
    ],
)

cc_test(
    name = "zero_copy_stream_test",
    size = "small",
    srcs = [
        "zero_copy_stream_test.cc",
    ],
    linkstatic = 1,
    deps = [
        ":zero_copy_stream",
        "//external:googletest_main",
        "//external:grpc",
        "//external:grpc++",
    ],
)
