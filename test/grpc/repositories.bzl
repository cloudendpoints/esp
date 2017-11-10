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

load("@io_bazel_rules_go//go:def.bzl", "go_repository")

def grpc_go_repositories():
    go_repository(
        name = "org_golang_google_grpc",
        commit = "91999f444f2aa89f2f873c8429a424701309bec4",
        importpath = "google.golang.org/grpc",
    )

    go_repository(
        name = "com_github_golang_protobuf",
        commit = "17ce1425424ab154092bbb43af630bd647f3bb0d",
        importpath = "github.com/golang/protobuf",
    )

    go_repository(
        name = "org_golang_google_genproto",
        commit = "ee236bd376b077c7a89f260c026c4735b195e459",
        importpath = "google.golang.org/genproto",
    )

    go_repository(
        name = "org_golang_x_net",
        commit = "66aacef3dd8a676686c7ae3716979581e8b03c47",
        importpath = "golang.org/x/net",
    )

    go_repository(
        name = "org_golang_x_oauth2",
        commit = "d89af98d7c6bba047c5a2622f36bc14b8766df85",
        importpath = "golang.org/x/oauth2",
    )

    go_repository(
        name = "com_google_cloud_go",
        commit = "2b74e2e25316cfd9e46b74e444cdeceb78786dc5",
        importpath = "cloud.google.com/go",
    )

    go_repository(
        name = "com_github_googleapis_gax_go",
        commit = "2cadd475a3e966ec9b77a21afc530dbacec6d613",
        importpath = "github.com/googleapis/gax-go",
    )

    go_repository(
        name = "org_golang_x_text",
        commit = "bd91bbf73e9a4a801adbfb97133c992678533126",
        importpath = "golang.org/x/text",
    )
