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
cc_binary(
    name = "esp_config_gen",
    srcs = [
        "esp_config_gen.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//external:api_manager",
        "//external:service_config",
        "//external:servicecontrol",
    ],
)

cc_binary(
    name = "read_server_config",
    srcs = [
        "read_server_config.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/api_manager:server_config_proto",
        "//src/api_manager/utils",
    ],
)

cc_binary(
    name = "auth_token_gen",
    srcs = [
        "auth_token_gen.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/api_manager/auth/lib",
    ],
)

cc_binary(
    name = "verify_token",
    srcs = [
        "verify_token.cc",
    ],
    deps = [
        "//src/api_manager/auth/lib",
    ],
)

cc_binary(
    name = "service_control_json_gen",
    srcs = [
        "service_control_json_gen.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//external:absl_strings",
        "//external:api_manager",
        "//external:servicecontrol",
        "@com_github_grpc_grpc//src/proto/grpc/testing:messages_proto",
    ],
)

cc_binary(
    name = "proto_pass_perf",
    srcs = [
        "proto_pass_perf.cc",
    ],
    deps = [
        "//external:api_manager",
        "//external:servicecontrol_client",
    ],
)
