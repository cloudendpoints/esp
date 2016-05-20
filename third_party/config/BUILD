licenses(["notice"])

load("@protobuf_git//:protobuf.bzl", "cc_proto_library")

cc_proto_library(
    name = "servicecontrol",
    srcs = [
        "google/api/servicecontrol/v1/check_error.proto",
        "google/api/servicecontrol/v1/distribution.proto",
        "google/api/servicecontrol/v1/log_entry.proto",
        "google/api/servicecontrol/v1/metric_value.proto",
        "google/api/servicecontrol/v1/operation.proto",
        "google/api/servicecontrol/v1/quota_properties.proto",
        "google/api/servicecontrol/v1/service_controller.proto",
        "google/logging/type/http_request.proto",
        "google/logging/type/log_severity.proto",
        "google/rpc/error_details.proto",
        "google/rpc/status.proto",
        "google/type/money.proto",
    ],
    include = ".",
    visibility = ["//visibility:public"],
    deps = [
        "//third_party:service_config",
    ],
    protoc = "//external:protoc",
    default_runtime = "//external:protobuf",
)
