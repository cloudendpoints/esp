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

load("@io_bazel_rules_pex//pex:pex_rules.bzl", "pex_binary")

cc_binary(
    name = "nginx-google",
    srcs = [
        "ngx_modules.c",
        "ngx_modules.h",
    ],
    linkstatic = 1,
    deps = [
        "//src/nginx:ngx_esp",
        "@nginx//:http",
        "@nginx//:http_access",
        "@nginx//:http_addition",
        "@nginx//:http_auth_basic",
        "@nginx//:http_auth_request",
        "@nginx//:http_autoindex",
        "@nginx//:http_browser",
        "@nginx//:http_charset",
        "@nginx//:http_dav",
        "@nginx//:http_empty_gif",
        "@nginx//:http_fastcgi",
        "@nginx//:http_flv",
        "@nginx//:http_geo",
        "@nginx//:http_gunzip",
        "@nginx//:http_gzip_filter",
        "@nginx//:http_gzip_static",
        "@nginx//:http_limit_conn",
        "@nginx//:http_limit_req",
        "@nginx//:http_map",
        "@nginx//:http_memcached",
        "@nginx//:http_mp4",
        "@nginx//:http_proxy",
        "@nginx//:http_random_index",
        "@nginx//:http_realip",
        "@nginx//:http_referer",
        "@nginx//:http_rewrite",
        "@nginx//:http_scgi",
        "@nginx//:http_secure_link",
        "@nginx//:http_split_clients",
        "@nginx//:http_ssi",
        "@nginx//:http_stub_status",
        "@nginx//:http_sub",
        "@nginx//:http_upstream_hash",
        "@nginx//:http_upstream_ip_hash",
        "@nginx//:http_upstream_keepalive",
        "@nginx//:http_upstream_least_conn",
        "@nginx//:http_userid",
        "@nginx//:http_uwsgi",
        "@nginx//:mail",
        "@nginx//:mail_imap",
        "@nginx//:mail_pop3",
        "@nginx//:mail_smtp",
        "@nginx//:stream",
        "@nginx//:stream_access",
        "@nginx//:stream_geo",
        "@nginx//:stream_limit_conn",
        "@nginx//:stream_map",
        "@nginx//:stream_return",
        "@nginx//:stream_split_clients",
        "@nginx//:stream_upstream_hash",
        "@nginx//:stream_upstream_least_conn",
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
        "//src/nginx:version_header",
        "@iap_jwt_verify_nginx//third_party/iap_jwt_verify_nginx/src:ngx_iap_jwt_verify",
        "@nginx//:http",
        "@nginx//:http_access",
        "@nginx//:http_addition",
        "@nginx//:http_auth_request",
        "@nginx//:http_autoindex",
        "@nginx//:http_browser",
        "@nginx//:http_charset",
        "@nginx//:http_geo",
        "@nginx//:http_gunzip",
        "@nginx//:http_gzip_filter",
        "@nginx//:http_gzip_static",
        "@nginx//:http_limit_conn",
        "@nginx//:http_limit_req",
        "@nginx//:http_map",
        "@nginx//:http_proxy",
        "@nginx//:http_realip",
        "@nginx//:http_referer",
        "@nginx//:http_rewrite",
        "@nginx//:http_split_clients",
        "@nginx//:http_stub_status",
        "@nginx//:http_sub",
        "@nginx//:http_upstream_hash",
        "@nginx//:http_upstream_ip_hash",
        "@nginx//:http_upstream_keepalive",
        "@nginx//:http_upstream_least_conn",
        "@nginx//:http_userid",
        "@nginx//:stream",
        "@nginx//:stream_access",
        "@nginx//:stream_geo",
        "@nginx//:stream_limit_conn",
        "@nginx//:stream_map",
        "@nginx//:stream_return",
        "@nginx//:stream_split_clients",
        "@nginx//:stream_upstream_hash",
        "@nginx//:stream_upstream_least_conn",
    ],
)

load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_tar", "pkg_deb")

genrule(
    name = "rename-nginx-esp",
    srcs = [":nginx-esp"],
    outs = ["nginx"],
    cmd = "cp $< $@",
)

genrule(
    name = "rename-copyright",
    srcs = [
        "//:LICENSE",
        "@nginx//:LICENSE",
        "@boringssl//:LICENSE",
        "@com_github_grpc_grpc//:LICENSE",
        "@com_github_grpc_grpc//third_party/nanopb:LICENSE.txt",
        "@nginx_pcre//:LICENCE",
        "@nginx_zlib//:README",
        "@protobuf_git//:LICENSE",
        "@servicecontrol_client_git//:LICENSE",
    ],
    outs = [
        "copyright",
    ],
    cmd = "cat $(location //:LICENSE) > $(@);" +
          "echo \"\n\" >> $(@);" +
          "echo \"This NGINX binary is statically linked against:\" >> $(@);" +
          "echo \"BoringSSL, gRPC, Nanopb, PCRE, Protocol Buffers," +
          " Istio Proxy, Service Control Client & zlib.\" >> $(@);" +
          "echo \"\n\nBoringSSL license:\n==================\n\" >> $(@);" +
          "cat $(location @boringssl//:LICENSE) >> $(@);" +
          "echo \"\n\ngRPC license:\n=============\n\" >> $(@);" +
          "cat $(location @com_github_grpc_grpc//:LICENSE) >> $(@);" +
          "echo \"\n\nNanopb license:\n===============\n\" >> $(@);" +
          "cat $(location @com_github_grpc_grpc//third_party/nanopb:LICENSE.txt) >> $(@);" +
          "echo \"\n\nNGINX license:\n==============\n\" >> $(@);" +
          "cat $(location @nginx//:LICENSE) >> $(@);" +
          "echo \"\n\nPCRE license:\n=============\n\" >> $(@);" +
          "cat $(location @nginx_pcre//:LICENCE) >> $(@);" +
          "echo \"\n\nProtocol Buffers license:\" >> $(@);" +
          "echo \"=========================\n\" >> $(@);" +
          "cat $(location @protobuf_git//:LICENSE) >> $(@);" +
          "echo \"\n\nService Control Client license:\" >> $(@);" +
          "echo \"=========================\n\" >> $(@);" +
          "cat $(location @servicecontrol_client_git//:LICENSE) >> $(@);" +
          "echo \"\n\nzlib license:\n=============\n\" >> $(@);" +
          "cat $(location @nginx_zlib//:README) | grep -A99 Copyright >> $(@)",
)

pkg_tar(
    name = "nginx-bin",
    files = [
        ":nginx",
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
    name = "esp-data",
    extension = "tar.gz",
    mode = "0644",
    deps = [
        # The first occurrence of the duplicate file will be picked. In other
        # words, /etc/nginx/nginx.conf from ":nginx-conf-override" will
        # override that from "@nginx_pkgoss//:debian_overlay".
        "//start_esp:nginx-conf-template",
        "//start_esp:server-conf-template",
        "//start_esp:nginx-initd-override",
        "@nginx_pkgoss//:debian_overlay",
        "//start_esp:esp-bin",
        ":nginx-bin",
        ":esp-copyright",
        "//start_esp:trusted-ca-certificates",
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
        "python",
    ],
    description_file = "description",
    homepage = "g.co/endpoints",
    maintainer = "Google Cloud Endpoints Runtime",
    package = "endpoints-runtime",
    postinst = "@nginx_pkgoss//:debian_postinst",
    postrm = "@nginx_pkgoss//:debian_postrm",
    preinst = "@nginx_pkgoss//:debian_preinst",
    prerm = "@nginx_pkgoss//:debian_prerm",
    priority = "extra",
    section = "httpd",
    version_file = "//src/nginx:version",
)

sh_test(
    name = "deb-test",
    srcs = [
        "deb-test.sh",
    ],
    data = [
        "testdata/control-expected",
        "testdata/copyright-expected",
        "testdata/data-expected",
        "testdata/deb-expected",
        ":endpoints-server-proxy.deb",
    ],
)
