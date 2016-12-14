# Copyright (C) Extensible Service Proxy Authors
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
################################################################################
#
# A Bazel (http://bazel.io) workspace for the Google Cloud Endpoints runtime.

ISTIO_PROXY = "db51059faa238bd7af39b754834ecc6acd11cca0"

git_repository(
    name = "nginx",
    commit = "cfce863dbe786e61e0e7a33376906b337da70c19",
    remote = "https://nginx.googlesource.com/nginx",
)

load("@nginx//:build.bzl", "nginx_repositories")

nginx_repositories(
    bind = True,
    nginx = "@nginx//",
)

git_repository(
    name = "istio_proxy_git",
    commit = ISTIO_PROXY,
    remote = "https://github.com/istio/proxy",
)

bind(
    name = "api_manager",
    actual = "@istio_proxy_git//contrib/endpoints/include:api_manager",
)

bind(
    name = "api_manager_status_proto",
    actual = "@istio_proxy_git//contrib/endpoints/src/api_manager:status_proto",
)

bind(
    name = "api_manager_status_proto_genproto",
    actual = "@istio_proxy_git//contrib/endpoints/src/api_manager:status_proto_genproto",
)

bind(
    name = "api_manager_auth_lib",
    actual = "@istio_proxy_git//contrib/endpoints/src/api_manager/auth/lib",
)

bind(
    name = "api_manager_utils",
    actual = "@istio_proxy_git//contrib/endpoints/src/api_manager/utils",
)

git_repository(
    name = "servicecontrol_client_git",
    commit = "d739d755365c6a13d0b4164506fd593f53932f5d",
    remote = "https://github.com/cloudendpoints/service-control-client-cxx.git",
)

bind(
    name = "servicecontrol_client",
    actual = "@servicecontrol_client_git//:service_control_client_lib",
)

# Required by gRPC.
bind(
    name = "libssl",
    actual = "@boringssl//:ssl",
)

# Though GRPC has BUILD file, our own BUILD.grpc file is needed since it contains
# more targets including testing server and client.
# To generate the BUILD.grpc file, cherry-pick
# https://github.com/grpc/grpc/pull/7556
# and run ./tools/buildgen/generate_projects.sh in GRPC repo.
new_git_repository(
    name = "grpc_git",
    build_file = "third_party/BUILD.grpc",
    commit = "d28417c856366df704200f544e72d31056931bce",
    init_submodules = True,
    remote = "https://github.com/grpc/grpc.git",
)

bind(
    name = "gpr",
    actual = "@grpc_git//:gpr",
)

bind(
    name = "grpc",
    actual = "@grpc_git//:grpc",
)

bind(
    name = "grpc_cpp_plugin",
    actual = "@grpc_git//:grpc_cpp_plugin",
)

bind(
    name = "grpc++",
    actual = "@grpc_git//:grpc++",
)

bind(
    name = "grpc_lib",
    actual = "@grpc_git//:grpc++_reflection",
)

# Workaround for Bazel > 0.4.0 since it needs newer protobuf.bzl from:
# https://github.com/google/protobuf/pull/2246
# Do not use this git_repository for anything else than protobuf.bzl
new_git_repository(
    name = "protobuf_bzl",
    # Injecting an empty BUILD file to prevent using any build target
    build_file_content = "",
    commit = "05090726144b6e632c50f47720ff51049bfcbef6",
    remote = "https://github.com/google/protobuf.git",
)

git_repository(
    name = "protobuf_git",
    commit = "a428e42072765993ff674fda72863c9f1aa2d268",  # v3.1.0
    remote = "https://github.com/google/protobuf.git",
)

bind(
    name = "protoc",
    actual = "@protobuf_git//:protoc",
)

bind(
    name = "protobuf",
    actual = "@protobuf_git//:protobuf",
)

bind(
    name = "cc_wkt_protos",
    actual = "@protobuf_git//:cc_wkt_protos",
)

bind(
    name = "cc_wkt_protos_genproto",
    actual = "@protobuf_git//:cc_wkt_protos_genproto",
)

bind(
    name = "protobuf_compiler",
    actual = "@protobuf_git//:protoc_lib",
)

bind(
    name = "protobuf_clib",
    actual = "@protobuf_git//:protobuf_lite",
)

new_git_repository(
    name = "nanopb_git",
    build_file = "third_party/BUILD.nanopb",
    commit = "f8ac463766281625ad710900479130c7fcb4d63b",
    remote = "https://github.com/nanopb/nanopb.git",
)

bind(
    name = "nanopb",
    actual = "@nanopb_git//:nanopb",
)

new_git_repository(
    name = "googletest_git",
    build_file = "third_party/BUILD.googletest",
    commit = "d225acc90bc3a8c420a9bcd1f033033c1ccd7fe0",
    remote = "https://github.com/google/googletest.git",
)

bind(
    name = "googletest",
    actual = "@googletest_git//:googletest",
)

bind(
    name = "googletest_main",
    actual = "@googletest_git//:googletest_main",
)

bind(
    name = "googletest_prod",
    actual = "@googletest_git//:googletest_prod",
)

new_git_repository(
    name = "googleapis_git",
    build_file = "third_party/BUILD.googleapis",
    commit = "db1d4547dc56a798915e0eb2c795585385922165",
    remote = "https://github.com/googleapis/googleapis.git",
)

bind(
    name = "servicecontrol",
    actual = "@googleapis_git//:servicecontrol",
)

bind(
    name = "servicecontrol_genproto",
    actual = "@googleapis_git//:servicecontrol_genproto",
)

bind(
    name = "service_config",
    actual = "@googleapis_git//:service_config",
)

bind(
    name = "cloud_trace",
    actual = "@googleapis_git//:cloud_trace",
)

git_repository(
    name = "gflags_git",
    commit = "fe57e5af4db74ab298523f06d2c43aa895ba9f98",  # 2016-07-22
    remote = "https://github.com/gflags/gflags",
)

bind(
    name = "gflags",
    actual = "@gflags_git//:gflags",
)

#
# Python rules
#

git_repository(
    name = "io_bazel_rules_pex",
    commit = "d4af3ca0a015e8b2d2a81a4df1df51bb0fa0bba0",
    remote = "https://github.com/benley/bazel_rules_pex.git",
)

load("@io_bazel_rules_pex//pex:pex_rules.bzl", "pex_repositories")

pex_repositories()

#
# Perl rules
#

git_repository(
    name = "io_bazel_rules_perl",
    commit = "f6211c2db1e54d0a30bc3c3a718f2b5d45f02a22",
    remote = "https://github.com/bazelbuild/rules_perl.git",
)

#
# Go rules
#

git_repository(
    name = "io_bazel_rules_go",
    commit = "3b13b2dba81e09ec213ccbd4da56ad332cb5d3dc",
    remote = "https://github.com/bazelbuild/rules_go.git",
)

load("@io_bazel_rules_go//go:def.bzl", "go_repositories")

go_repositories()

new_git_repository(
    name = "github_com_golang_protobuf",
    build_file = "third_party/BUILD.golang_protobuf",
    commit = "8616e8ee5e20a1704615e6c8d7afcdac06087a67",
    remote = "https://github.com/golang/protobuf.git",
)

new_git_repository(
    name = "github_com_spf13_pflag",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/spf13")
go_library(name="pflag", srcs=glob(include=["*.go"], exclude=["*_test.go"]),
    visibility = ["//visibility:public"])""",
    commit = "6fd2ff4ff8dfcdf5556fbdc0ac0284408274b1a7",
    remote = "https://github.com/spf13/pflag.git",
)

new_git_repository(
    name = "github_com_spf13_cobra",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/spf13")
go_library(
    name = "cobra",
    srcs = [
        "bash_completions.go",
        "cobra.go",
        "command.go",
        "command_notwin.go",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@github_com_spf13_pflag//:pflag",
    ],
)
    """,
    commit = "9c28e4bbd74e5c3ed7aacbc552b2cab7cfdfe744",
    remote = "https://github.com/spf13/cobra.git",
)

new_git_repository(
    name = "github_com_golang_glog",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/golang")
go_library(
    name = "glog",
    srcs = ["glog.go", "glog_file.go"],
    visibility = ["//visibility:public"],
)
    """,
    commit = "44145f04b68cf362d9c4df2182967c2275eaefed",
    remote = "https://github.com/golang/glog.git",
)

new_git_repository(
    name = "github_com_google_gofuzz",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/google")
go_library(
    name = "gofuzz",
    srcs = ["fuzz.go", "doc.go"],
    visibility = ["//visibility:public"],
)
    """,
    commit = "bbcb9da2d746f8bdbd6a936686a0a6067ada0ec5",
    remote = "https://github.com/google/gofuzz",
)

new_git_repository(
    name = "go_spew",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/davecgh/go-spew")
go_library(
    name = "spew",
    srcs = glob(include = ["spew/*.go"], exclude = ["spew/*_test.go", "spew/bypasssafe.go"]),
    visibility = ["//visibility:public"],
)
    """,
    commit = "5215b55f46b2b919f50a1df0eaa5886afe4e3b3d",
    remote = "https://github.com/davecgh/go-spew",
)

new_git_repository(
    name = "pborman_uuid",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/pborman")
go_library(
    name = "uuid",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """,
    commit = "ca53cad383cad2479bbba7f7a1a05797ec1386e4",
    remote = "https://github.com/pborman/uuid",
)

new_git_repository(
    name = "go_codec",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/ugorji/go")
go_library(
    name = "codec",
    srcs = glob(include = ["codec/*.go"],
      exclude = ["codec/*_test.go",
                 "codec/helper_not_unsafe.go",
                 "codec/fast-path.not.go"]),
    visibility = ["//visibility:public"],
)
    """,
    commit = "f1f1a805ed361a0e078bb537e4ea78cd37dcf065",
    remote = "https://github.com/ugorji/go.git",
)

new_git_repository(
    name = "go_yaml",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("gopkg.in")
go_library(
    name = "yaml.v2",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """,
    commit = "53feefa2559fb8dfa8d81baad31be332c97d6c77",
    remote = "https://github.com/go-yaml/yaml.git",
)

new_git_repository(
    name = "ghodss_yaml",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/ghodss")
go_library(
    name = "yaml",
    srcs = ["yaml.go", "fields.go"],
    visibility = ["//visibility:public"],
    deps = ["@go_yaml//:yaml.v2"],
)
    """,
    commit = "73d445a93680fa1a78ae23a5839bad48f32ba1ee",
    remote = "https://github.com/ghodss/yaml",
)

new_git_repository(
    name = "github_com_gogo_protobuf",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/gogo/protobuf")
go_library(
    name = "proto",
    srcs = glob(include = ["proto/*.go"], exclude = ["proto/*_test.go", "proto/pointer_reflect.go"]),
    visibility = ["//visibility:public"],
)
go_library(
    name = "sortkeys",
    srcs = ["sortkeys/sortkeys.go"],
    visibility = ["//visibility:public"],
)
    """,
    commit = "e18d7aa8f8c624c915db340349aad4c49b10d173",
    remote = "https://github.com/gogo/protobuf.git",
)

new_git_repository(
    name = "github_com_x_crypto",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("golang.org/x/crypto")
go_library(
    name = "ssh/terminal",
    visibility = ["//visibility:public"],
    srcs = ["ssh/terminal/terminal.go", "ssh/terminal/util.go", "ssh/terminal/util_linux.go"],
)
    """,
    commit = "1f22c0103821b9390939b6776727195525381532",
    remote = "https://github.com/golang/crypto.git",
)

new_git_repository(
    name = "github_com_x_text",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("golang.org/x/text")
go_library(
    name = "unicode/cldr",
    visibility = ["//visibility:public"],
    srcs = glob(include = ["unicode/cldr/*.go"], exclude = ["unicode/cldr/*_test.go", "unicode/cldr/makexml.go"]),
)
go_library(
    name = "transform",
    visibility = ["//visibility:public"],
    srcs = ["transform/transform.go"],
)
go_library(
    name = "unicode/norm",
    visibility = ["//visibility:public"],
    srcs = glob(include = ["unicode/norm/*.go"], exclude = [
        "unicode/norm/*_test.go",
        "unicode/norm/triegen.go",
        "unicode/norm/maketables.go"
    ]),
    deps = [":transform"],
)
go_library(
    name = "internal/gen",
    visibility = ["//visibility:public"],
    srcs = ["internal/gen/gen.go", "internal/gen/code.go"],
    deps = [
        ":unicode/cldr",
    ],
)
go_library(
    name = "internal/tag",
    visibility = ["//visibility:public"],
    srcs = ["internal/tag/tag.go"],
)
go_library(
    name = "language",
    visibility = ["//visibility:public"],
    srcs = glob(include=["language/*.go"],exclude=[
        "language/*_test.go",
        "language/go1_2.go",
        "language/gen_common.go",
        "language/gen_index.go",
        "language/maketables.go",
    ]),
    deps = [":internal/tag", ":internal/gen", ":unicode/cldr"],
)
go_library(
    name = "cases",
    visibility = ["//visibility:public"],
    srcs = glob(include=["cases/*.go"],exclude=[
        "cases/*_test.go",
        "cases/gen.go",
        "cases/gen_trieval.go",
    ]),
    deps = [":language", ":transform", ":unicode/norm"],
)
go_library(
    name = "runes",
    visibility = ["//visibility:public"],
    srcs = ["runes/cond.go", "runes/runes.go"],
    deps = [":transform"],
)
go_library(
    name = "width",
    visibility = ["//visibility:public"],
    srcs = ["width/kind_string.go", "width/tables.go", "width/transform.go", "width/trieval.go", "width/width.go"],
    deps = [":transform"],
)
go_library(
    name = "unicode/bidi",
    visibility = ["//visibility:public"],
    srcs = ["unicode/bidi/bidi.go", "unicode/bidi/core.go", "unicode/bidi/prop.go", "unicode/bidi/tables.go",
        "unicode/bidi/trieval.go", "unicode/bidi/bracket.go"],
)
go_library(
    name = "secure/bidirule",
    visibility = ["//visibility:public"],
    srcs = ["secure/bidirule/bidirule.go"],
    deps = [":transform", ":unicode/bidi"],
)
go_library(
    name = "secure/precis",
    visibility = ["//visibility:public"],
    srcs = glob(include = ["secure/precis/*.go"], exclude = [
        "secure/precis/*_test.go",
        "secure/precis/gen.go",
        "secure/precis/gen_trieval.go",
    ]),
    deps = [
        ":cases",
        ":internal/gen",
        ":runes",
        ":transform",
        ":unicode/norm",
        ":width",
        ":secure/bidirule",
    ],
)
""",
    commit = "2910a502d2bf9e43193af9d68ca516529614eed3",
    remote = "https://github.com/golang/text.git",
)

new_git_repository(
    name = "github_com_x_net",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("golang.org/x/net")
go_library(
    name = "context",
    srcs = ["context/context.go", "context/go17.go"],
    visibility = ["//visibility:public"],
)
go_library(
    name = "context/ctxhttp",
    srcs = ["context/ctxhttp/ctxhttp.go"],
    visibility = ["//visibility:public"],
    deps = ["context"],
)
go_library(
    name = "idna",
    srcs = ["idna/idna.go", "idna/punycode.go"],
    visibility = ["//visibility:public"],
)
go_library(
    name = "lex/httplex",
    srcs = ["lex/httplex/httplex.go"],
    visibility = ["//visibility:public"],
)
go_library(
    name = "http2/hpack",
    srcs = glob(include = ["http2/hpack/*.go"], exclude = ["http2/hpack/*_test.go"]),
    visibility = ["//visibility:public"],
)
go_library(
    name = "http2",
    srcs = glob(include = ["http2/*.go"], exclude = ["http2/*_test.go", "http2/go17.go", "http2/not_go16.go"]),
    visibility = ["//visibility:public"],
    deps = [
        ":http2/hpack",
        ":lex/httplex",
    ],
)
    """,
    commit = "e90d6d0afc4c315a0d87a568ae68577cc15149a0",
    remote = "https://github.com/golang/net.git",
)

new_git_repository(
    name = "docker",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/docker/distribution")
go_library(
    name = "reference",
    srcs = ["reference/reference.go", "reference/regexp.go"],
    visibility = ["//visibility:public"],
    deps = [":digest"],
)
go_library(
    name = "digest",
    srcs = glob(include=["digest/*.go"],exclude=["digest/*_test.go"]),
    visibility = ["//visibility:public"],
)
    """,
    commit = "cd27f179f2c10c5d300e6d09025b538c475b0d51",
    remote = "https://github.com/docker/distribution.git",
)

new_git_repository(
    name = "blang_semver",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/blang")
go_library(
    name = "semver",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """,
    commit = "31b736133b98f26d5e078ec9eb591666edfd091f",
    remote = "https://github.com/blang/semver.git",
)

new_git_repository(
    name = "juju_ratelimit",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/juju")
go_library(
    name = "ratelimit",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """,
    commit = "77ed1c8a01217656d2080ad51981f6e99adaa177",
    remote = "https://github.com/juju/ratelimit.git",
)

new_git_repository(
    name = "jonboulle_clockwork",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/jonboulle")
go_library(name="clockwork", srcs=["clockwork.go"], visibility=["//visibility:public"])
""",
    commit = "72f9bd7c4e0c2a40055ab3d0f09654f730cce982",
    remote = "https://github.com/jonboulle/clockwork.git",
)

new_git_repository(
    name = "go_inf",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("gopkg.in")
go_library(
    name = "inf.v0",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """,
    remote = "https://github.com/go-inf/inf.git",
    tag = "v0.9.0",
)

new_git_repository(
    name = "imdario_mergo",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/imdario")
go_library(
    name = "mergo",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """,
    commit = "6633656539c1639d9d78127b7d47c622b5d7b6dc",
    remote = "https://github.com/imdario/mergo.git",
)

new_git_repository(
    name = "groupcache_lru",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/golang/groupcache")
go_library(
    name = "lru",
    srcs = ["lru/lru.go"],
    visibility = ["//visibility:public"],
)
    """,
    commit = "02826c3e79038b59d737d3b1c0a1d937f71a4433",
    remote = "https://github.com/golang/groupcache.git",
)

new_git_repository(
    name = "puerkitobio_purell",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/PuerkitoBio")
go_library(
    name = "purell",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [
        "@puerkitobio_urlesc//:urlesc",
        "@github_com_x_net//:idna",
        "@github_com_x_text//:secure/precis",
        "@github_com_x_text//:unicode/norm",
    ]
)
    """,
    commit = "8a290539e2e8629dbc4e6bad948158f790ec31f4",
    remote = "https://github.com/PuerkitoBio/purell.git",
)

new_git_repository(
    name = "puerkitobio_urlesc",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/PuerkitoBio")
go_library(
    name = "urlesc",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
)
    """,
    commit = "5bd2802263f21d8788851d5305584c82a5c75d7e",
    remote = "https://github.com/PuerkitoBio/urlesc.git",
)

new_git_repository(
    name = "mailru_easyjson",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/mailru/easyjson")
go_library(
    name = "buffer",
    srcs = glob(include = ["buffer/*.go"], exclude = ["buffer/*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [
    ]
)
go_library(
    name = "jlexer",
    srcs = glob(include = ["jlexer/*.go"], exclude = ["jlexer/*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [
    ]
)
go_library(
    name = "jwriter",
    srcs = glob(include = ["jwriter/*.go"], exclude = ["jwriter/*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [
        ":buffer",
    ]
)
    """,
    commit = "d5b7844b561a7bc640052f1b935f7b800330d7e0",
    remote = "https://github.com/mailru/easyjson.git",
)

new_git_repository(
    name = "go_openapi_swag",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/go-openapi")
go_library(
    name = "swag",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [
        "@mailru_easyjson//:jlexer",
        "@mailru_easyjson//:jwriter",
    ]
)
    """,
    commit = "1d0bd113de87027671077d3c71eb3ac5d7dbba72",
    remote = "https://github.com/go-openapi/swag.git",
)

new_git_repository(
    name = "go_openapi_jsonpointer",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/go-openapi")
go_library(
    name = "jsonpointer",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [
        "@go_openapi_swag//:swag",
    ]
)
    """,
    commit = "46af16f9f7b149af66e5d1bd010e3574dc06de98",
    remote = "https://github.com/go-openapi/jsonpointer.git",
)

new_git_repository(
    name = "go_openapi_jsonreference",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/go-openapi")
go_library(
    name = "jsonreference",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [
        "@go_openapi_swag//:swag",
        "@puerkitobio_purell//:purell",
        "@go_openapi_jsonpointer//:jsonpointer",
    ]
)
    """,
    commit = "13c6e3589ad90f49bd3e3bbe2c2cb3d7a4142272",
    remote = "https://github.com/go-openapi/jsonreference.git",
)

new_git_repository(
    name = "go_openapi_spec",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/go-openapi")
go_library(
    name = "spec",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [
        "@go_openapi_swag//:swag",
        "@go_openapi_jsonpointer//:jsonpointer",
        "@go_openapi_jsonreference//:jsonreference",
    ],
)
    """,
    commit = "6aced65f8501fe1217321abf0749d354824ba2ff",
    remote = "https://github.com/go-openapi/spec.git",
)

new_git_repository(
    name = "howeyc_gopass",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/howeyc")
go_library(
    name = "gopass",
    srcs = ["pass.go", "terminal.go"],
    visibility = ["//visibility:public"],
    deps = [
        "@github_com_x_crypto//:ssh/terminal",
    ]
)
    """,
    commit = "3ca23474a7c7203e0a0a070fd33508f6efdb9b3d",
    remote = "https://github.com/howeyc/gopass.git",
)

new_git_repository(
    name = "coreos_pkg",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/coreos/pkg")
go_library(
    name = "httputil",
    srcs = glob(include = ["httputil/*.go"], exclude = ["httputil/*_test.go"]),
    visibility = ["//visibility:public"],
)
go_library(
    name = "timeutil",
    srcs = glob(include = ["timeutil/*.go"], exclude = ["timeutil/*_test.go"]),
    visibility = ["//visibility:public"],
)
go_library(
    name = "health",
    srcs = glob(include = ["health/*.go"], exclude = ["health/*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [":httputil"],
)
""",
    commit = "fa29b1d70f0beaddd4c7021607cc3c3be8ce94b8",
    remote = "https://github.com/coreos/pkg.git",
)

new_git_repository(
    name = "coreos_go_oidc",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/coreos/go-oidc")
go_library(
    name = "jose",
    srcs = glob(include = ["jose/*.go"], exclude = ["jose/*_test.go"]),
    visibility = ["//visibility:public"],
)
go_library(
    name = "key",
    srcs = glob(include = ["key/*.go"], exclude = ["key/*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [
        ":jose",
        "@jonboulle_clockwork//:clockwork",
        "@coreos_pkg//:health",
        "@coreos_pkg//:timeutil",
    ],
)
go_library(
    name = "http",
    srcs = glob(include = ["http/*.go"], exclude = ["http/*_test.go"]),
    visibility = ["//visibility:public"],
)
go_library(
    name = "oauth2",
    srcs = glob(include = ["oauth2/*.go"], exclude = ["oauth2/*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [":http"],
)
go_library(
    name = "oidc",
    srcs = glob(include = ["oidc/*.go"], exclude = ["oidc/*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [":http", ":jose", ":key", ":oauth2", "@coreos_pkg//:timeutil",
        "@jonboulle_clockwork//:clockwork",
    ],
)
""",
    commit = "5644a2f50e2d2d5ba0b474bc5bc55fea1925936d",
    remote = "https://github.com/coreos/go-oidc.git",
)

new_git_repository(
    name = "go_restful",
    build_file_content = """
load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@io_bazel_rules_go//go:def.bzl", "go_library")
go_prefix("github.com/emicklei")
go_library(
    name = "go-restful",
    srcs = glob(include = ["*.go"], exclude = ["*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [":go-restful/log"],
)
go_library(
    name = "go-restful/swagger",
    srcs = glob(include = ["swagger/*.go"], exclude = ["swagger/*_test.go"]),
    visibility = ["//visibility:public"],
    deps = [":go-restful", ":go-restful/log"],
)
go_library(
    name = "go-restful/log", srcs = ["log/log.go"],
    visibility = ["//visibility:public"],
)
    """,
    commit = "89ef8af493ab468a45a42bb0d89a06fccdd2fb22",
    remote = "https://github.com/emicklei/go-restful.git",
)

new_git_repository(
    name = "github_com_kubernetes_client_go",
    build_file = "third_party/BUILD.kubernetes",
    commit = "c589d0c9f0d81640c518354c7bcae77d99820aa3",
    remote = "https://github.com/kubernetes/client-go.git",
)

new_git_repository(
    name = "google_api_go_client",
    build_file = "third_party/BUILD.google_api_go_client",
    commit = "adba394bac5800ff2e620d040e9401528f5b7615",
    remote = "https://github.com/google/google-api-go-client.git",
)

new_git_repository(
    name = "golang_oauth2",
    build_file = "third_party/BUILD.golang_oauth2",
    commit = "3c3a985cb79f52a3190fbc056984415ca6763d",
    remote = "https://github.com/golang/oauth2.git",
)

new_git_repository(
    name = "google_cloud_go",
    build_file = "third_party/BUILD.google_cloud_go",
    commit = "290ed46a0684cc372f475ca8f36b63aa3066978e",
    remote = "https://github.com/GoogleCloudPlatform/google-cloud-go.git",
)
