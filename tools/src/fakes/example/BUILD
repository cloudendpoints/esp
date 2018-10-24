load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library")

go_library(
    name = "go_default_library",
    srcs = ["main.go"],
    importpath = "fakes/example",
    visibility = ["//visibility:private"],
    deps = [
        "//tools/src/fakes:go_default_library",
        "//tools/src/google/api/servicecontrol/v1:go_default_library",
        "@com_github_golang_protobuf//proto:go_default_library",
    ],
)

go_binary(
    name = "example",
    embed = [":go_default_library"],
    visibility = ["//visibility:public"],
)
