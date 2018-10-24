load("@io_bazel_rules_go//go:def.bzl", "go_library")

go_library(
    name = "go_default_library",
    srcs = [
        "check_error.pb.go",
        "distribution.pb.go",
        "log_entry.pb.go",
        "metric_value.pb.go",
        "operation.pb.go",
        "quota_properties.pb.go",
        "service_controller.pb.go",
    ],
    importpath = "google/api/servicecontrol/v1",
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_golang_protobuf//proto:go_default_library",
        "//tools/src/google/api:go_default_library",
        "//tools/src/google/logging/type:go_default_library",
        "//tools/src/google/protobuf:go_default_library",
        "//tools/src/google/rpc:go_default_library",
        "//tools/src/google/type:go_default_library",
        "@io_bazel_rules_go//proto/wkt:any_go_proto",
        "@io_bazel_rules_go//proto/wkt:struct_go_proto",
        "@io_bazel_rules_go//proto/wkt:timestamp_go_proto",
    ],
)
