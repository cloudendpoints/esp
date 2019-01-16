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
def cares_repositories(bind = True):
    BUILD = """
cc_library(
    name = "ares",
    srcs = [
        "ares__close_sockets.c",
        "ares__get_hostent.c",
        "ares__read_line.c",
        "ares__timeval.c",
        "ares_cancel.c",
        "ares_create_query.c",
        "ares_data.c",
        "ares_destroy.c",
        "ares_expand_name.c",
        "ares_expand_string.c",
        "ares_fds.c",
        "ares_free_hostent.c",
        "ares_free_string.c",
        "ares_getenv.c",
        "ares_gethostbyaddr.c",
        "ares_gethostbyname.c",
        "ares_getnameinfo.c",
        "ares_getopt.c",
        "ares_getsock.c",
        "ares_init.c",
        "ares_library_init.c",
        "ares_llist.c",
        "ares_mkquery.c",
        "ares_nowarn.c",
        "ares_options.c",
        "ares_parse_a_reply.c",
        "ares_parse_aaaa_reply.c",
        "ares_parse_mx_reply.c",
        "ares_parse_naptr_reply.c",
        "ares_parse_ns_reply.c",
        "ares_parse_ptr_reply.c",
        "ares_parse_soa_reply.c",
        "ares_parse_srv_reply.c",
        "ares_parse_txt_reply.c",
        "ares_platform.c",
        "ares_process.c",
        "ares_query.c",
        "ares_search.c",
        "ares_send.c",
        "ares_strcasecmp.c",
        "ares_strdup.c",
        "ares_strerror.c",
        "ares_timeout.c",
        "ares_version.c",
        "ares_writev.c",
        "bitncmp.c",
        "inet_net_pton.c",
        "inet_ntop.c",
        "windows_port.c",
    ],
    hdrs = [
        "ares_config.h",
        "ares.h",
        "ares_build.h",
        "ares_data.h",
        "ares_dns.h",
        "ares_getenv.h",
        "ares_getopt.h",
        "ares_inet_net_pton.h",
        "ares_iphlpapi.h",
        "ares_ipv6.h",
        "ares_library_init.h",
        "ares_llist.h",
        "ares_nowarn.h",
        "ares_platform.h",
        "ares_private.h",
        "ares_rules.h",
        "ares_setup.h",
        "ares_strcasecmp.h",
        "ares_strdup.h",
        "ares_version.h",
        "ares_writev.h",
        "bitncmp.h",
        "nameser.h",
        "setup_once.h",
    ],
    copts = [
        "-DHAVE_CONFIG_H",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
)
genrule(
    name = "config",
    srcs = glob(["**/*"]),
    outs = ["ares_config.h"],
    cmd = "pushd external/cares_git ; ./buildconf ; ./configure ; cp ares_config.h ../../$@",
    visibility = ["//visibility:public"],
)
genrule(
    name = "ares_build",
    srcs = [
        "ares_build.h.dist",
    ],
    outs = [
        "ares_build.h",
    ],
    cmd = "cp $(SRCS) $@",
    visibility = ["//visibility:public"],
)
"""

    native.new_git_repository(
        name = "cares_git",
        remote = "https://github.com/c-ares/c-ares.git",
        commit = "7691f773af79bf75a62d1863fd0f13ebf9dc51b1",  # v1.12.0
        build_file_content = BUILD,
    )

    if bind:
        native.bind(
            name = "cares",
            actual = "@cares_git//:ares",
        )

def protobuf_repositories(bind = True):
    native.git_repository(
        name = "protobuf_git",
        commit = "48cb18e5c419ddd23d9badcfe4e9df7bde1979b2",  # same as grpc
        remote = "https://github.com/google/protobuf.git",
    )

    if bind:
        native.bind(
            name = "protoc",
            actual = "@protobuf_git//:protoc",
        )

        native.bind(
            name = "protobuf",
            actual = "@protobuf_git//:protobuf",
        )

        native.bind(
            name = "cc_wkt_protos",
            actual = "@protobuf_git//:cc_wkt_protos",
        )

        native.bind(
            name = "cc_wkt_protos_genproto",
            actual = "@protobuf_git//:cc_wkt_protos_genproto",
        )

        native.bind(
            name = "protobuf_compiler",
            actual = "@protobuf_git//:protoc_lib",
        )

        native.bind(
            name = "protobuf_clib",
            actual = "@protobuf_git//:protoc_lib",
        )

def googletest_repositories(bind = True):
    BUILD = """
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

cc_library(
    name = "googletest",
    srcs = [
        "googletest/src/gtest-all.cc",
        "googlemock/src/gmock-all.cc",
    ],
    hdrs = glob([
        "googletest/include/**/*.h",
        "googlemock/include/**/*.h",
        "googletest/src/*.cc",
        "googletest/src/*.h",
        "googlemock/src/*.cc",
    ]),
    includes = [
        "googlemock",
        "googletest",
        "googletest/include",
        "googlemock/include",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "googletest_main",
    srcs = ["googlemock/src/gmock_main.cc"],
    visibility = ["//visibility:public"],
    linkopts = [
        "-lpthread",
    ],
    deps = [":googletest"],
)

cc_library(
    name = "googletest_prod",
    hdrs = [
        "googletest/include/gtest/gtest_prod.h",
    ],
    includes = [
        "googletest/include",
    ],
    visibility = ["//visibility:public"],
)
"""
    native.new_git_repository(
        name = "googletest_git",
        build_file_content = BUILD,
        commit = "d225acc90bc3a8c420a9bcd1f033033c1ccd7fe0",
        remote = "https://github.com/google/googletest.git",
    )

    if bind:
        native.bind(
            name = "googletest",
            actual = "@googletest_git//:googletest",
        )

        native.bind(
            name = "googletest_main",
            actual = "@googletest_git//:googletest_main",
        )

        native.bind(
            name = "googletest_prod",
            actual = "@googletest_git//:googletest_prod",
        )

def transcoding_repositories(bind = True):
    native.git_repository(
        name = "httpjson_transcoding",
        commit = "6c54b75dbd294e1e264e3f9476ffb52be8763cd3",
        remote = "https://github.com/grpc-ecosystem/grpc-httpjson-transcoding.git",
    )

    if bind:
        native.bind(
            name = "transcoding",
            actual = "@httpjson_transcoding//src:transcoding",
        )

        native.bind(
            name = "path_matcher",
            actual = "@httpjson_transcoding//src:path_matcher",
        )

def zlib_repositories(bind = True):
    BUILD = """
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

licenses(["notice"])

exports_files(["README"])

cc_library(
    name = "zlib",
    srcs = [
        "adler32.c",
        "crc32.c",
        "crc32.h",
        "deflate.c",
        "deflate.h",
        "infback.c",
        "inffast.c",
        "inffast.h",
        "inffixed.h",
        "inflate.c",
        "inflate.h",
        "inftrees.c",
        "inftrees.h",
        "trees.c",
        "trees.h",
        "zconf.h",
        "zutil.c",
        "zutil.h",
    ],
    hdrs = [
        "zlib.h",
    ],
    copts = [
        "-Wno-shift-negative-value",
        "-Wno-unknown-warning-option",
    ],
    defines = [
        "Z_SOLO",
    ],
    visibility = [
        "//visibility:public",
    ],
)
"""

    native.new_git_repository(
        name = "zlib_git",
        build_file_content = BUILD,
        commit = "50893291621658f355bc5b4d450a8d06a563053d",  # v1.2.8
        remote = "https://github.com/madler/zlib.git",
    )

    native.bind(
        name = "zlib",
        actual = "@zlib_git//:zlib",
    )

def nanopb_repositories(bind = True):
    BUILD = """
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

licenses(["notice"])

exports_files(["LICENSE.txt"])

cc_library(
    name = "nanopb",
    srcs = [
        "pb.h",
        "pb_common.c",
        "pb_common.h",
        "pb_decode.c",
        "pb_decode.h",
        "pb_encode.c",
        "pb_encode.h",
    ],
    hdrs = [":includes"],
    visibility = [
        "//visibility:public",
    ],
)

genrule(
    name = "includes",
    srcs = [
        "pb.h",
        "pb_common.h",
        "pb_decode.h",
        "pb_encode.h",
    ],
    outs = [
        "third_party/nanopb/pb.h",
        "third_party/nanopb/pb_common.h",
        "third_party/nanopb/pb_decode.h",
        "third_party/nanopb/pb_encode.h",
    ],
    cmd = "mkdir -p $(@D)/third_party/nanopb && cp $(SRCS) $(@D)/third_party/nanopb",
)
"""

    native.new_git_repository(
        name = "nanopb_git",
        build_file_content = BUILD,
        commit = "f8ac463766281625ad710900479130c7fcb4d63b",
        remote = "https://github.com/nanopb/nanopb.git",
    )

    native.bind(
        name = "nanopb",
        actual = "@nanopb_git//:nanopb",
    )

def googleapis_repositories(protobuf_repo = "@protobuf_git//", bind = True):
    BUILD = """
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

licenses(["notice"])

load("@protobuf_git//:protobuf.bzl", "cc_proto_library")

exports_files(glob(["google/**"]))

cc_proto_library(
    name = "http_api_protos",
    srcs = [
        "google/api/annotations.proto",
        "google/api/http.proto",
    ],
    include = ".",
    visibility = ["//visibility:public"],
    deps = [
        "//external:cc_wkt_protos",
    ],
    protoc = "//external:protoc",
    default_runtime = "//external:protobuf",
)

cc_proto_library(
    name = "servicecontrol",
    srcs = [
        "google/api/servicecontrol/v1/check_error.proto",
        "google/api/servicecontrol/v1/distribution.proto",
        "google/api/servicecontrol/v1/log_entry.proto",
        "google/api/servicecontrol/v1/metric_value.proto",
        "google/api/servicecontrol/v1/operation.proto",
        "google/api/servicecontrol/v1/quota_controller.proto",
        "google/api/servicecontrol/v1/service_controller.proto",
        "google/api/servicemanagement/v1/servicemanager.proto",
        "google/api/servicemanagement/v1/resources.proto",
        "google/logging/type/http_request.proto",
        "google/logging/type/log_severity.proto",
        "google/api/config_change.proto",
        "google/longrunning/operations.proto",
        "google/rpc/error_details.proto",
        "google/rpc/status.proto",
        "google/type/money.proto",
    ],
    include = ".",
    visibility = ["//visibility:public"],
    deps = [
        ":service_config",
    ],
    protoc = "//external:protoc",
    default_runtime = "//external:protobuf",
)

cc_proto_library(
    name = "service_config",
    srcs = [
        "google/api/auth.proto",
        "google/api/backend.proto",
        "google/api/billing.proto",
        "google/api/consumer.proto",
        "google/api/context.proto",
        "google/api/control.proto",
        "google/api/documentation.proto",
        "google/api/endpoint.proto",
        "google/api/label.proto",
        "google/api/log.proto",
        "google/api/logging.proto",
        "google/api/metric.proto",
        "google/api/experimental/experimental.proto",
        "google/api/experimental/authorization_config.proto",
        "google/api/monitored_resource.proto",
        "google/api/monitoring.proto",
        "google/api/quota.proto",
        "google/api/service.proto",
        "google/api/source_info.proto",
        "google/api/system_parameter.proto",
        "google/api/usage.proto",
    ],
    include = ".",
    visibility = ["//visibility:public"],
    deps = [
        ":http_api_protos",
        "//external:cc_wkt_protos",
    ],
    protoc = "//external:protoc",
    default_runtime = "//external:protobuf",
)

cc_proto_library(
    name = "cloud_trace",
    srcs = [
        "google/devtools/cloudtrace/v1/trace.proto",
    ],
    include = ".",
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"],
    deps = [
        ":service_config",
        "//external:cc_wkt_protos",
    ],
)
""".format(protobuf_repo)

    # Update the SHA due to backend.proto updates, to support
    # backend routing.
    native.new_git_repository(
        name = "googleapis_git",
        commit = "bf12fe4fd40797ab6701555cdff0bac4c18374e8",  # Jan 15, 2019
        remote = "https://github.com/JLXIA/googleapis.git",
        build_file_content = BUILD,
    )

    if bind:
        native.bind(
            name = "servicecontrol",
            actual = "@googleapis_git//:servicecontrol",
        )

        native.bind(
            name = "servicecontrol_genproto",
            actual = "@googleapis_git//:servicecontrol_genproto",
        )

        native.bind(
            name = "service_config",
            actual = "@googleapis_git//:service_config",
        )

        native.bind(
            name = "cloud_trace",
            actual = "@googleapis_git//:cloud_trace",
        )

        native.bind(
            name = "http_api_protos",
            actual = "@googleapis_git//:http_api_protos",
        )

def servicecontrol_client_repositories(bind = True):
    googleapis_repositories(bind = bind)

    native.git_repository(
        name = "servicecontrol_client_git",
        commit = "8189638f8e2c410010befe5dbd81e267a15e3e17",
        remote = "https://github.com/cloudendpoints/service-control-client-cxx.git",
    )

    if bind:
        native.bind(
            name = "servicecontrol_client",
            actual = "@servicecontrol_client_git//:service_control_client_lib",
        )
        native.bind(
            name = "quotacontrol",
            actual = "@servicecontrol_client_git//proto:quotacontrol",
        )
        native.bind(
            name = "quotacontrol_genproto",
            actual = "@servicecontrol_client_git//proto:quotacontrol_genproto",
        )
