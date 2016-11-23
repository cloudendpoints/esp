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

load("//third_party/nginx:build.bzl", "nginx_repositories")

nginx_repositories(
    bind = True,
    nginx = "@//third_party/nginx",
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
    commit = "6c1d6d4067364a21f8ffefa3401b213d652bf121",
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
# Go dependencies
#

git_repository(
    name = "io_bazel_rules_perl",
    commit = "f6211c2db1e54d0a30bc3c3a718f2b5d45f02a22",
    remote = "https://github.com/bazelbuild/rules_perl.git",
)

git_repository(
    name = "io_bazel_rules_go",
    commit = "878185a8d66cb4a3fb93602d8e8bc2f50cd69616",
    remote = "https://github.com/bazelbuild/rules_go.git",
)

load(
    "@io_bazel_rules_go//go:def.bzl",
    "new_go_repository",
    "go_repositories",
)

go_repositories()

# Generated by wtool (https://github.com/bazelbuild/rules_go/blob/master/go/tools/wtool/main.go)
# wtool com_github_golang_protobuf
# wtool com_github_spf13_pflag
# wtool com_github_spf13_cobra
# wtool com_github_golang_glog
# wtool com_github_google_gofuzz
# wtool com_github_davecgh_go_spew
# wtool com_github_pborman_uuid
# wtool com_github_ugorji_go
# wtool -asis gopkg.in/yaml.v2
# wtool com_github_ghodss_yaml
# wtool com_github_gogo_protobuf
# wtool org_golang_x_crypto
# wtool org_golang_x_text
# wtool org_golang_x_net
# wtool com_github_docker_distribution
# wtool com_github_blang_semver
# wtool com_github_juju_ratelimit
# wtool com_github_jonboulle_clockwork
# wtool -asis gopkg.in/inf.v0
# wtool com_github_imdario_mergo
# wtool com_github_golang_groupcache
# wtool com_github_PuerkitoBio_purell
# wtool com_github_PuerkitoBio_urlesc
# wtool com_github_mailru_easyjson
# wtool -asis github.com/go-openapi/spec
# wtool -asis github.com/go-openapi/swag
# wtool -asis github.com/go-openapi/jsonpointer
# wtool -asis github.com/go-openapi/jsonreference
# wtool com_github_howeyc_gopass
# wtool com_github_coreos_pkg
# wtool -asis github.com/coreos/go-oidc
# wtool com_github_kubernetes_client-go
# wtool org_golang_google_api
# wtool org_golang_x_oauth2
# wtool com_google_cloud_go
# wtool com_github_googleapis_gax_go
# wtool org_golang_google_grpc
# wtool com_github_emicklei_go_restful

new_go_repository(
    name = "com_github_golang_protobuf",
    commit = "8ee79997227bf9b34611aee7946ae64735e6fd93",
    importpath = "github.com/golang/protobuf",
)

new_go_repository(
    name = "com_github_spf13_pflag",
    commit = "5ccb023bc27df288a957c5e994cd44fd19619465",
    importpath = "github.com/spf13/pflag",
)

new_go_repository(
    name = "com_github_spf13_cobra",
    commit = "9495bc009a56819bdb0ddbc1a373e29c140bc674",
    importpath = "github.com/spf13/cobra",
)

new_go_repository(
    name = "com_github_golang_glog",
    commit = "23def4e6c14b4da8ac2ed8007337bc5eb5007998",
    importpath = "github.com/golang/glog",
)

new_go_repository(
    name = "com_github_google_gofuzz",
    commit = "44d81051d367757e1c7c6a5a86423ece9afcf63c",
    importpath = "github.com/google/gofuzz",
)

new_go_repository(
    name = "com_github_davecgh_go_spew",
    commit = "346938d642f2ec3594ed81d874461961cd0faa76",
    importpath = "github.com/davecgh/go-spew",
)

new_go_repository(
    name = "com_github_pborman_uuid",
    commit = "3d4f2ba23642d3cfd06bd4b54cf03d99d95c0f1b",
    importpath = "github.com/pborman/uuid",
)

new_go_repository(
    name = "com_github_ugorji_go",
    commit = "faddd6128c66c4708f45fdc007f575f75e592a3c",
    importpath = "github.com/ugorji/go",
)

new_go_repository(
    name = "in_gopkg_yaml_v2",
    commit = "a5b47d31c556af34a302ce5d659e6fea44d90de0",
    importpath = "gopkg.in/yaml.v2",
)

new_go_repository(
    name = "com_github_ghodss_yaml",
    commit = "bea76d6a4713e18b7f5321a2b020738552def3ea",
    importpath = "github.com/ghodss/yaml",
)

new_go_repository(
    name = "com_github_gogo_protobuf",
    commit = "8d70fb3182befc465c4a1eac8ad4d38ff49778e2",
    importpath = "github.com/gogo/protobuf",
)

new_go_repository(
    name = "org_golang_x_crypto",
    commit = "ede567c8e044a5913dad1d1af3696d9da953104c",
    importpath = "golang.org/x/crypto",
)

new_go_repository(
    name = "org_golang_x_text",
    commit = "a263ba8db058568bb9beba166777d9c9dbe75d68",
    importpath = "golang.org/x/text",
)

new_go_repository(
    name = "org_golang_x_net",
    commit = "4971afdc2f162e82d185353533d3cf16188a9f4e",
    importpath = "golang.org/x/net",
)

new_go_repository(
    name = "com_github_docker_distribution",
    commit = "a6bf3dd064f15598166bca2d66a9962a9555139e",
    importpath = "github.com/docker/distribution",
)

new_go_repository(
    name = "com_github_blang_semver",
    commit = "60ec3488bfea7cca02b021d106d9911120d25fe9",
    importpath = "github.com/blang/semver",
)

new_go_repository(
    name = "com_github_juju_ratelimit",
    commit = "77ed1c8a01217656d2080ad51981f6e99adaa177",
    importpath = "github.com/juju/ratelimit",
)

new_go_repository(
    name = "com_github_jonboulle_clockwork",
    commit = "bcac9884e7502bb2b474c0339d889cb981a2f27f",
    importpath = "github.com/jonboulle/clockwork",
)

new_go_repository(
    name = "com_github_go-inf_inf",
    commit = "3887ee99ecf07df5b447e9b00d9c0b2adaa9f3e4",
    importpath = "github.com/go-inf/inf",
)

new_go_repository(
    name = "com_github_imdario_mergo",
    commit = "50d4dbd4eb0e84778abe37cefef140271d96fade",
    importpath = "github.com/imdario/mergo",
)

new_go_repository(
    name = "com_github_golang_groupcache",
    commit = "a6b377e3400b08991b80d6805d627f347f983866",
    importpath = "github.com/golang/groupcache",
)

new_go_repository(
    name = "com_github_PuerkitoBio_purell",
    commit = "0bcb03f4b4d0a9428594752bd2a3b9aa0a9d4bd4",
    importpath = "github.com/PuerkitoBio/purell",
)

new_go_repository(
    name = "com_github_PuerkitoBio_urlesc",
    commit = "5bd2802263f21d8788851d5305584c82a5c75d7e",
    importpath = "github.com/PuerkitoBio/urlesc",
)

new_go_repository(
    name = "com_github_mailru_easyjson",
    commit = "159cdb893c982e3d1bc6450322fedd514f9c9de3",
    importpath = "github.com/mailru/easyjson",
)

new_go_repository(
    name = "com_github_go_openapi_swag",
    commit = "3b6d86cd965820f968760d5d419cb4add096bdd7",
    importpath = "github.com/go-openapi/swag",
)

new_go_repository(
    name = "com_github_go_openapi_jsonreference",
    commit = "36d33bfe519efae5632669801b180bf1a245da3b",
    importpath = "github.com/go-openapi/jsonreference",
)

new_go_repository(
    name = "com_github_go_openapi_spec",
    commit = "f7ae86df5bc115a2744343016c789a89f065a4bd",
    importpath = "github.com/go-openapi/spec",
)

new_go_repository(
    name = "com_github_howeyc_gopass",
    commit = "f5387c492211eb133053880d23dfae62aa14123d",
    importpath = "github.com/howeyc/gopass",
)

new_go_repository(
    name = "com_github_coreos_pkg",
    commit = "447b7ec906e523386d9c53be15b55a8ae86ea944",
    importpath = "github.com/coreos/pkg",
)

new_go_repository(
    name = "com_github_coreos_go_oidc",
    commit = "5a7f09ab5787e846efa7f56f4a08b6d6926d08c4",
    importpath = "github.com/coreos/go-oidc",
)

new_go_repository(
    name = "com_github_kubernetes_client-go",
    commit = "ecd05810bd98f1ccb9a4558871cb0de3aefd50b4",
    importpath = "github.com/kubernetes/client-go",
)

new_go_repository(
    name = "org_golang_google_api",
    commit = "c8d75a8ec737f9b8b1ed2676c28feedbe21f543f",
    importpath = "google.golang.org/api",
)

new_go_repository(
    name = "org_golang_x_oauth2",
    commit = "d5040cddfc0da40b408c9a1da4728662435176a9",
    importpath = "golang.org/x/oauth2",
)

new_go_repository(
    name = "com_google_cloud_go",
    commit = "36c2fc7fd284e6705d2997b6bc76fb491a87e90a",
    importpath = "cloud.google.com/go",
)

new_go_repository(
    name = "com_github_googleapis_gax_go",
    commit = "da06d194a00e19ce00d9011a13931c3f6f6887c7",
    importpath = "github.com/googleapis/gax-go",
)

new_go_repository(
    name = "org_golang_google_grpc",
    commit = "63bd55dfbf781b183216d2dd4433a659c947648a",
    importpath = "google.golang.org/grpc",
)

new_go_repository(
    name = "io_k8s_client_go",
    commit = "843f7c4f28b1f647f664f883697107d5c02c5acc", # v1.5.0
    importpath = "k8s.io/client-go",
)

new_go_repository(
    name = "in_gopkg_inf_v0",
    commit = "3887ee99ecf07df5b447e9b00d9c0b2adaa9f3e4",
    importpath = "gopkg.in/inf.v0",
)

new_go_repository(
    name = "com_github_emicklei_go_restful",
    commit = "858e58f98abd32bc4d164a08e8d7ac169ae876e2",
    importpath = "github.com/emicklei/go-restful",
)

new_go_repository(
    name = "com_github_go_openapi_jsonpointer",
    commit = "8d96a2dc61536b690bd36b2e9df0b3c0b62825b2",
    importpath = "github.com/go-openapi/jsonpointer",
)
