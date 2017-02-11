load("@io_bazel_rules_go//go:def.bzl", "new_go_repository")

def grpc_go_repositories():
    new_go_repository(
        name = "org_golang_google_grpc",
        commit = "708a7f9f3283aa2d4f6132d287d78683babe55c8",
        importpath = "google.golang.org/grpc",
    )

    new_go_repository(
        name = "com_github_golang_protobuf",
        commit = "8ee79997227bf9b34611aee7946ae64735e6fd93",
        importpath = "github.com/golang/protobuf",
    )

    new_go_repository(
        name = "org_golang_x_net",
        commit = "a689eb3bc4b53af70390acc3cf68c9f549b6b8d6",
        importpath = "golang.org/x/net",
    )

    new_go_repository(
        name = "org_golang_x_oauth2",
        commit = "de0725b330ab43c1a3d6c84d961cf01183783f1e",
        importpath = "golang.org/x/oauth2",
    )

    new_go_repository(
        name = "com_google_cloud_go",
        commit = "513b07bb7468fa6d8c59519f35b66456bce959b5",
        importpath = "cloud.google.com/go",
    )

    new_go_repository(
        name = "com_github_googleapis_gax_go",
        commit = "da06d194a00e19ce00d9011a13931c3f6f6887c7",
        importpath = "github.com/googleapis/gax-go",
    )
