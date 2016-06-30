# Copyright (C) Endpoints Server Proxy Authors
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
cc_binary(
    name = "nginx-google",
    srcs = [
        "ngx_modules.c",
    ],
    linkstatic = 1,
    deps = [
        "//src/nginx:ngx_esp",
        "//third_party/nginx:http",
        "//third_party/nginx:http_access",
        "//third_party/nginx:http_addition",
        "//third_party/nginx:http_auth_basic",
        "//third_party/nginx:http_auth_request",
        "//third_party/nginx:http_autoindex",
        "//third_party/nginx:http_browser",
        "//third_party/nginx:http_charset",
        "//third_party/nginx:http_dav",
        "//third_party/nginx:http_empty_gif",
        "//third_party/nginx:http_fastcgi",
        "//third_party/nginx:http_flv",
        "//third_party/nginx:http_geo",
        "//third_party/nginx:http_gunzip",
        "//third_party/nginx:http_gzip_filter",
        "//third_party/nginx:http_gzip_static",
        "//third_party/nginx:http_limit_conn",
        "//third_party/nginx:http_limit_req",
        "//third_party/nginx:http_map",
        "//third_party/nginx:http_memcached",
        "//third_party/nginx:http_mp4",
        "//third_party/nginx:http_proxy",
        "//third_party/nginx:http_random_index",
        "//third_party/nginx:http_realip",
        "//third_party/nginx:http_referer",
        "//third_party/nginx:http_rewrite",
        "//third_party/nginx:http_scgi",
        "//third_party/nginx:http_secure_link",
        "//third_party/nginx:http_split_clients",
        "//third_party/nginx:http_ssi",
        "//third_party/nginx:http_stub_status",
        "//third_party/nginx:http_sub",
        "//third_party/nginx:http_upstream_hash",
        "//third_party/nginx:http_upstream_ip_hash",
        "//third_party/nginx:http_upstream_keepalive",
        "//third_party/nginx:http_upstream_least_conn",
        "//third_party/nginx:http_userid",
        "//third_party/nginx:http_uwsgi",
        "//third_party/nginx:mail",
        "//third_party/nginx:mail_imap",
        "//third_party/nginx:mail_pop3",
        "//third_party/nginx:mail_smtp",
        "//third_party/nginx:stream",
        "//third_party/nginx:stream_access",
        "//third_party/nginx:stream_limit_conn",
        "//third_party/nginx:stream_upstream_hash",
        "//third_party/nginx:stream_upstream_least_conn",
    ],
)

cc_binary(
    name = "nginx-esp",
    srcs = [
        "ngx_modules.c",
    ],
    linkstatic = 1,
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//src/nginx:ngx_esp",
        "//third_party/nginx:http",
        "//third_party/nginx:http_access",
        "//third_party/nginx:http_addition",
        "//third_party/nginx:http_auth_request",
        "//third_party/nginx:http_autoindex",
        "//third_party/nginx:http_browser",
        "//third_party/nginx:http_charset",
        "//third_party/nginx:http_geo",
        "//third_party/nginx:http_gunzip",
        "//third_party/nginx:http_gzip_filter",
        "//third_party/nginx:http_gzip_static",
        "//third_party/nginx:http_limit_conn",
        "//third_party/nginx:http_limit_req",
        "//third_party/nginx:http_map",
        "//third_party/nginx:http_proxy",
        "//third_party/nginx:http_realip",
        "//third_party/nginx:http_referer",
        "//third_party/nginx:http_rewrite",
        "//third_party/nginx:http_stub_status",
        "//third_party/nginx:http_sub",
        "//third_party/nginx:http_upstream_hash",
        "//third_party/nginx:http_upstream_ip_hash",
        "//third_party/nginx:http_upstream_keepalive",
        "//third_party/nginx:http_upstream_least_conn",
        "//third_party/nginx:http_userid",
    ],
)

load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_tar", "pkg_deb")

genrule(
    name = "rename-mginx-esp",
    srcs = [":nginx-esp"],
    outs = ["nginx"],
    cmd = "cp $< $@",
)

genrule(
    name = "rename-copyright",
    srcs = [
        "//:LICENSE",
    ],
    outs = [
        "copyright",
    ],
    cmd = "cp $(<) $(@)",
)

pkg_tar(
    name = "esp-bin",
    files = [
        ":debug_nginx.sh",
        ":fetch_service_config.sh",
        ":nginx",
        ":nginx-debug",
    ],
    mode = "0755",
    package_dir = "/usr/sbin",
    strip_prefix = ".",
)

pkg_tar(
    name = "esp-copyright",
    files = [
        ":copyright",
    ],
    mode = "0644",
    package_dir = "/usr/share/doc/nginx-google",
    strip_prefix = ".",
)

pkg_tar(
    name = "nginx-initd-override",
    files = [
        "//third_party/nginx-init.d:nginx",
    ],
    mode = "0755",
    package_dir = "/etc/init.d",
)

pkg_tar(
    name = "nginx-conf-override",
    files = [
        "//src/nginx/main:nginx.conf",
    ],
    mode = "0644",
    package_dir = "/etc/nginx",
    strip_prefix = ".",
)

pkg_tar(
    name = "trusted-ca-certificates",
    files = [
        "//src/nginx/main:trusted-ca-certificates.crt",
    ],
    mode = "0644",
    package_dir = "/etc/nginx",
    strip_prefix = ".",
)

pkg_tar(
    name = "esp-data",
    extension = "tar.gz",
    mode = "0644",
    deps = [
        # The first occurrence of the duplicate file will be picked. In other
        # words, /etc/nginx/nginx.conf from ":nginx-conf-override" will
        # override that from "@nginx_pkgoss_git//:debian_overlay".
        ":nginx-conf-override",
        ":nginx-initd-override",
        "@nginx_pkgoss_git//:debian_overlay",
        ":esp-bin",
        ":esp-copyright",
        ":trusted-ca-certificates",
    ],
)

pkg_deb(
    name = "endpoints-server-proxy",
    architecture = "amd64",
    built_using_file = "built_using",
    data = ":esp-data",
    depends = [
        "libc6 (>= 2.17)",
        "libgcc1 (>= 1:4.1.1)",
        "libstdc++6 (>= 4.9)",
        "libuuid1 (>= 2.16)",
        "lsb-base",
        "adduser",
    ],
    description_file = "description",
    homepage = "g.co/endpoints",
    maintainer = "Google Cloud Endpoints Runtime",
    package = "endpoints-runtime",
    postinst = "@nginx_pkgoss_git//:debian_postinst",
    postrm = "@nginx_pkgoss_git//:debian_postrm",
    preinst = "@nginx_pkgoss_git//:debian_preinst",
    prerm = "@nginx_pkgoss_git//:debian_prerm",
    priority = "extra",
    section = "httpd",
    version_file = "//include:version",
)

sh_test(
    name = "deb-test",
    srcs = [
        "deb-test.sh",
    ],
    data = [
        "testdata/control-expected",
        "testdata/data-expected",
        "testdata/deb-expected",
        ":endpoints-server-proxy.deb",
        "//:LICENSE",
    ],
)
