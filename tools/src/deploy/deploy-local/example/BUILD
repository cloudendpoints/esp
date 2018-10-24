load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library")

go_library(
    name = "go_default_library",
    srcs = ["deploy-local-test.go"],
    importpath = "deploy/deploy-local/example",
    visibility = ["//visibility:private"],
    deps = ["//tools/src/deploy/deploy-local:go_default_library"],
)

go_binary(
    name = "example",
    embed = [":go_default_library"],
    visibility = ["//visibility:public"],
)
