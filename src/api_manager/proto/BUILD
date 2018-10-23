# Copyright (C) Extensible Service Proxy Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
#

load("@io_bazel_rules_go//go:def.bzl", "go_library")
load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")

proto_library(
    name = "server_config_proto_file",
    srcs = ["server_config.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_protobuf//:empty_proto",
        "@com_google_protobuf//:struct_proto",
    ],
)

cc_proto_library(
    name = "server_config_proto",
    deps = [":server_config_proto_file"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "api_manager_status_proto_file",
    srcs = ["api_manager_status.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_protobuf//:empty_proto",
        "@com_google_protobuf//:struct_proto",
    ],
)

cc_proto_library(
    name = "api_manager_status_proto",
    deps = [":api_manager_status_proto_file"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "security_rules_proto_file",
    srcs = ["security_rules.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_protobuf//:empty_proto",
        "@com_google_protobuf//:struct_proto",
    ],
)

cc_proto_library(
    name = "security_rules_proto",
    deps = [":security_rules_proto_file"],
    visibility = ["//visibility:public"],
)

