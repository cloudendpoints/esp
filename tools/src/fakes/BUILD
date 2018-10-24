load("@io_bazel_rules_go//go:def.bzl", "go_library")

go_library(
    name = "go_default_library",
    srcs = [
        "config.go",
        "controller.go",
        "request_data.go",
        "request_parser.go",
    ],
    importpath = "fakes",
    visibility = ["//visibility:public"],
    deps = [
        "//tools/src/google/api/servicecontrol/v1:go_default_library",
        "@com_github_golang_protobuf//proto:go_default_library",
    ],
)
