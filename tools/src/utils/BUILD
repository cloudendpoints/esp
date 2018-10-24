load("@io_bazel_rules_go//go:def.bzl", "go_library", "go_test")
load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")

go_library(
    name = "go_default_library",
    srcs = [
        "auth_token_gen.go",
        "command.go",
        "nginx_config.go",
        "service_config.go",
        "service_control.go",
        "status.go",
        "test_env_path.go",
    ],
    importpath = "utils",
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_golang_protobuf//proto:go_default_library",
        "@com_github_golang_protobuf//ptypes/struct:go_default_library",
        "//tools/src/google/api/servicecontrol/v1:go_default_library",
        "//tools/src/google/logging/type:go_default_library",
    ],
)

go_test(
    name = "go_default_test",
    srcs = [
        "auth_token_gen_test.go",
        "nginx_config_test.go",
        "service_config_test.go",
        "service_control_test.go",
    ],
    embed = [":go_default_library"],
    deps = [
        "@com_github_golang_protobuf//proto:go_default_library",
    ],
)
