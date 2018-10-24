load("@io_bazel_rules_go//go:def.bzl", "go_library")

go_library(
    name = "go_default_library",
    srcs = [
        "annotations.pb.go",
        "auth.pb.go",
        "backend.pb.go",
        "billing.pb.go",
        "consumer.pb.go",
        "context.pb.go",
        "control.pb.go",
        "documentation.pb.go",
        "http.pb.go",
        "label.pb.go",
        "log.pb.go",
        "logging.pb.go",
        "metric.pb.go",
        "monitored_resource.pb.go",
        "monitoring.pb.go",
        "service.pb.go",
        "system_parameter.pb.go",
        "usage.pb.go",
    ],
    importpath = "google/api",
    visibility = ["//visibility:public"],
    deps = [       
        "//tools/src/google/protobuf:go_default_library",
        "@com_github_golang_protobuf//proto:go_default_library",
        "@io_bazel_rules_go//proto/wkt:any_go_proto",
        "@io_bazel_rules_go//proto/wkt:wrappers_go_proto",
    ],
)
