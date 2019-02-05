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

load("@bazel_gazelle//:deps.bzl", "go_repository")

def grpc_go_repositories():
    go_repository(
        name = "org_golang_google_grpc",
        commit = "9bf8ea0a8282ebecd1aa474c926e3028f5c22a4c",
        importpath = "google.golang.org/grpc",
    )

    go_repository(
        name = "com_github_golang_protobuf",
        commit = "fec3b39b059c0f88fa6b20f5ed012b1aa203a8b4",
        importpath = "github.com/golang/protobuf",
    )

    go_repository(
        name = "org_golang_google_genproto",
        commit = "bb3573be0c484136831138976d444b8754777aff",
        importpath = "google.golang.org/genproto",
    )

    go_repository(
        name = "org_golang_x_net",
        commit = "513929065c19401a1c7b76ecd942f9f86a0c061b",
        importpath = "golang.org/x/net",
    )

    go_repository(
        name = "org_golang_x_oauth2",
        commit = "f047394b6d14284165300fd82dad67edb3a4d7f6",
        importpath = "golang.org/x/oauth2",
    )

    go_repository(
        name = "com_google_cloud_go",
        commit = "0625e1e4bfc1aa7a07d6285541fa9020feab1013",
        importpath = "cloud.google.com/go",
    )

    go_repository(
        name = "com_github_googleapis_gax_go",
        commit = "9af46dd5a1713e8b5cd71106287eba3cefdde50b",
        importpath = "github.com/googleapis/gax-go",
    )

    go_repository(
        name = "org_golang_x_text",
        commit = "19e51611da83d6be54ddafce4a4af510cb3e9ea4",
        importpath = "golang.org/x/text",
    )
