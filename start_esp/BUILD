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
###############################################################################
#

load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_tar", "pkg_deb")
load("@io_bazel_rules_pex//pex:pex_rules.bzl", "pex_binary")

package(default_visibility = ["//visibility:public"])

pkg_tar(
    name = "esp-bin",
    srcs = [
        ":debug_nginx.sh",
        ":fetch_service_config.sh",
        ":nginx-debug",
        ":start_esp",
    ],
    mode = "0755",
    package_dir = "/usr/sbin",
    strip_prefix = ".",
)

pex_binary(
    name = "start_esp",
    srcs = [
        "fetch_service_config.py",
        "start_esp.py",
    ],
    main = "start_esp.py",
    reqs = [
        "certifi",
        "mako>=1.0.4",
        "google-auth>=1.6.3",
        "pyasn1>=0.1.9",
        "pyasn1-modules>=0.0.8",
        "urllib3>=1.16",
    ],
)

pkg_tar(
    name = "nginx-conf-template",
    srcs = [
        ":nginx-auto.conf.template",
    ],
    mode = "0644",
    package_dir = "/etc/nginx",
    strip_prefix = ".",
)

pkg_tar(
    name = "server-conf-template",
    srcs = [
        ":server-auto.conf.template",
        ":server_config.pb.txt",
    ],
    mode = "0644",
    package_dir = "/etc/nginx",
    strip_prefix = ".",
)

pkg_tar(
    name = "trusted-ca-certificates",
    srcs = [
        ":trusted-ca-certificates.crt",
    ],
    mode = "0644",
    package_dir = "/etc/nginx",
    strip_prefix = ".",
)

pkg_tar(
    name = "nginx-initd-override",
    srcs = [
        ":nginx-init.d/nginx",
    ],
    mode = "0755",
    package_dir = "/etc/init.d",
)
