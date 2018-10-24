load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library")

go_library(
    name = "go_default_library",
    srcs = [
        "main.go",
    ],
    importpath = "fakes/server",
    visibility = [
        "//visibility:private",
    ],
    deps = [
        "//tools/src/fakes:go_default_library",
    ],
)

go_binary(
    name = "server",
    embed = [
        ":go_default_library",
    ],
    visibility = [
        "//visibility:public",
    ],
)

go_binary(
    name = "fakes_server",
    embed = [
        ":go_default_library",
    ],
    visibility = [
        "//visibility:public",
    ],
)
