load("@io_bazel_rules_go//go:def.bzl", "go_library", "go_test")

go_library(
    name = "go_default_library",
    srcs = [
        "backend.go",
        #"bookstore.go",
        "esp.go",
        "metadata_server.go",
        "pk_server.go",
        "service_control.go",
        "setup.go",
    ],
    importpath = "t",
    visibility = ["//visibility:public"],
    deps = [
        "//tools/src/deploy/deploy-local:go_default_library",
        "//tools/src/fakes:go_default_library",
        "//tools/src/utils:go_default_library",
    ],
)

go_test(
    name = "go_default_test",
    srcs = ["t_test.go"],
    data = [
        "//tools/src/fakes/server:fakes_server",
        "//src/nginx/main:nginx-esp",
        "//src/nginx/t:matching-client-secret.json",
        "//src/tools:auth_token_gen",
    ],
    embed = [":go_default_library"],
    deps = ["//tools/src/utils:go_default_library"],
)
