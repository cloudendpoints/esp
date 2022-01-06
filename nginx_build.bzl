# Copyright (C) 2015-2018 Google Inc.
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

load(
    "@bazel_tools//tools/build_defs/repo:git.bzl",
    "git_repository",
    "new_git_repository",
)
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

_common_copts = [
    "-fno-common",
    "-fvisibility=hidden",
    "-Wall",
    "-Werror",
    "-Wextra",
    "-Wformat=2",
    "-Wlong-long",
    "-Wpointer-arith",
    "-Wshadow",
    "-Wno-deprecated-declarations",
    "-Wno-unused-parameter",
]

nginx_copts = _common_copts + [
    "-Wmissing-prototypes",
    "-Wold-style-definition",
    "-Wstrict-prototypes",
]

nginx_cxxopts = _common_copts + [
    "-Wmissing-declarations",
]

_NGX_BROTLI_BUILD_FILE = """
# Copyright (C) 2015-2018 Google Inc.
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

licenses(["notice"])  # BSD license

exports_files(["LICENSE"])

load("{nginx}:build.bzl", "nginx_copts")

cc_library(
    name = "http_brotli_filter",
    srcs = [
        "src/ngx_http_brotli_filter_module.c",
    ],
    copts = nginx_copts,
    defines = [
        "NGX_HTTP_BROTLI_FILTER",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//external:brotli_enc",
        "{nginx}:core",
        "{nginx}:http",
    ],
)

cc_library(
    name = "http_brotli_static",
    srcs = [
        "src/ngx_http_brotli_static_module.c",
    ],
    copts = nginx_copts,
    defines = [
        "NGX_HTTP_BROTLI_STATIC",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "{nginx}:core",
        "{nginx}:http",
    ],
)

cc_binary(
    name = "nginx",
    srcs = [
        "{nginx}:modules",
    ],
    copts = nginx_copts,
    deps = [
        ":http_brotli_filter",
        ":http_brotli_static",
        "{nginx}:core",
    ],
)
"""

_PCRE_BUILD_FILE = """
# Copyright (C) 2015-2018 Google Inc.
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

licenses(["notice"])

exports_files(["LICENCE"])

genrule(
    name = "config_h",
    srcs = [
        "config.h.generic",
    ],
    outs = [
        "config.h",
    ],
    cmd = "cp -p $(<) $(@)",
)

genrule(
    name = "pcre_h",
    srcs = [
        "pcre.h.generic",
    ],
    outs = [
        "pcre.h",
    ],
    cmd = "cp -p $(<) $(@)",
)

genrule(
    name = "pcre_chartables_c",
    srcs = [
        "pcre_chartables.c.dist",
    ],
    outs = [
        "pcre_chartables.c",
    ],
    cmd = "cp -p $(<) $(@)",
)

cc_library(
    name = "sljit",
    srcs = [
        "sljit/sljitConfig.h",
        "sljit/sljitConfigInternal.h",
        "sljit/sljitLir.h",
    ],
    hdrs = [
        "sljit/sljitExecAllocator.c",
        "sljit/sljitLir.c",
        "sljit/sljitNativeX86_64.c",
        "sljit/sljitNativeX86_common.c",
        "sljit/sljitUtils.c",
    ],
)

cc_library(
    name = "pcre",
    srcs = [
        "config.h",
        "pcre_byte_order.c",
        "pcre_chartables.c",
        "pcre_compile.c",
        "pcre_config.c",
        "pcre_dfa_exec.c",
        "pcre_exec.c",
        "pcre_fullinfo.c",
        "pcre_get.c",
        "pcre_globals.c",
        "pcre_internal.h",
        "pcre_jit_compile.c",
        "pcre_maketables.c",
        "pcre_newline.c",
        "pcre_ord2utf8.c",
        "pcre_refcount.c",
        "pcre_study.c",
        "pcre_tables.c",
        "pcre_ucd.c",
        "pcre_valid_utf8.c",
        "pcre_version.c",
        "pcre_xclass.c",
        "ucp.h",
    ],
    hdrs = [
        "pcre.h",
    ],
    copts = [
        "-DHAVE_CONFIG_H",
        "-DHAVE_MEMMOVE",
        "-DHAVE_STDINT_H",
        "-DNO_RECURSE",
        "-DSUPPORT_JIT",
        "-DSUPPORT_PCRE8",
        "-DSUPPORT_UCP",
        "-DSUPPORT_UTF",
        "-Wno-maybe-uninitialized",
        "-Wno-unknown-warning-option",
    ],
    includes = [
        ".",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":sljit",
    ],
)
"""

_PKGOSS_BUILD_FILE = """
# Copyright (C) 2015-2018 Google Inc.
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

licenses(["notice"])

exports_files(["README"])

load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_tar")

genrule(
    name = "debian_nginx_preinst",
    srcs = [
        "debian/nginx.preinst",
    ],
    outs = [
        "nginx.preinst",
    ],
    cmd = "sed -e 's|#DEBHELPER#||g'" +
          " < $(<) > $(@)",
)

filegroup(
    name = "debian_preinst",
    srcs = [
        "nginx.preinst",
    ],
    visibility = [
        "//visibility:public",
    ],
)

genrule(
    name = "debian_nginx_postinst",
    srcs = [
        "debian/nginx.postinst",
    ],
    outs = [
        "nginx.postinst",
    ],
    cmd = "sed -e 's|#DEBHELPER#|" +
          "if [ -x \\"/etc/init.d/nginx\\" ]; then\\\\n" +
          "\\\\tupdate-rc.d nginx defaults >/dev/null \\|\\| exit $$?\\\\n" +
          "fi\\\\n" +
          "|g'" +
          " < $(<) > $(@)",
)

filegroup(
    name = "debian_postinst",
    srcs = [
        "nginx.postinst",
    ],
    visibility = [
        "//visibility:public",
    ],
)

genrule(
    name = "debian_nginx_prerm",
    srcs = [
        "debian/nginx.prerm",
    ],
    outs = [
        "nginx.prerm",
    ],
    cmd = "sed -e 's|#DEBHELPER#||g'" +
          " < $(<) > $(@)",
)

filegroup(
    name = "debian_prerm",
    srcs = [
        "nginx.prerm",
    ],
    visibility = [
        "//visibility:public",
    ],
)

genrule(
    name = "debian_nginx_postrm",
    srcs = [
        "debian/nginx.postrm",
    ],
    outs = [
        "nginx.postrm",
    ],
    cmd = "sed -e 's|#DEBHELPER#|" +
          "if [ \\"$$1\\" = \\"purge\\" ] ; then\\\\n" +
          "\\\\tupdate-rc.d nginx remove >/dev/null\\\\n" +
          "fi\\\\n" +
          "\\\\n" +
          "if [ -d /run/systemd/system ] ; then\\\\n" +
          "\\\\tsystemctl --system daemon-reload >/dev/null \\|\\| true\\\\n" +
          "fi\\\\n" +
          "|g'" +
          " < $(<) > $(@)",
)

filegroup(
    name = "debian_postrm",
    srcs = [
        "nginx.postrm",
    ],
    visibility = [
        "//visibility:public",
    ],
)

genrule(
    name = "debian_etc_default_nginx",
    srcs = [
        "debian/nginx.default",
    ],
    outs = [
        "etc/default/nginx",
    ],
    cmd = "cp -p $(<) $(@)",
)

genrule(
    name = "debian_etc_init_d_nginx",
    srcs = [
        "debian/nginx.init.in",
    ],
    outs = [
        "etc/init.d/nginx",
    ],
    cmd = "sed -e 's|%%PROVIDES%%|nginx|g'" +
          " -e 's|%%DEFAULTSTART%%|2 3 4 5|g'" +
          " -e 's|%%DEFAULTSTOP%%|0 1 6|g'" +
          " < $(<) > $(@)",
)

genrule(
    name = "debian_etc_logrotate_d_nginx",
    srcs = [
        "debian/nginx.logrotate",
    ],
    outs = [
        "etc/logrotate.d/nginx",
    ],
    cmd = "cp -p $(<) $(@)",
)

genrule(
    name = "debian_etc_nginx_conf_d_default_conf",
    srcs = [
        "debian/nginx.vh.default.conf",
    ],
    outs = [
        "etc/nginx/conf.d/default.conf",
    ],
    cmd = "cp -p $(<) $(@)",
)

genrule(
    name = "debian_etc_nginx_nginx_conf",
    srcs = [
        "debian/nginx.conf",
    ],
    outs = [
        "etc/nginx/nginx.conf",
    ],
    cmd = "cp -p $(<) $(@)",
)

genrule(
    name = "debian_usr_share_man_man8_nginx_8",
    srcs = [
        "{nginx}:docs/man/nginx.8",
    ],
    outs = [
        "usr/share/man/man8/nginx.8",
    ],
    cmd = "sed -e 's|%%PREFIX%%|/etc/nginx|g'" +
          " -e 's|%%CONF_PATH%%|/etc/nginx/nginx.conf|g'" +
          " -e 's|%%ERROR_LOG_PATH%%|/var/log/nginx/error.log|g'" +
          " -e 's|%%PID_PATH%%|/var/run/nginx.pid|g'" +
          " < $(<) > $(@)",
)

genrule(
    name = "debian_var_cache_nginx",
    outs = [
        "var/cache/nginx/.empty",
    ],
    cmd = "touch $(@)",
)

genrule(
    name = "debian_var_log_nginx",
    outs = [
        "var/log/nginx/.empty",
    ],
    cmd = "touch $(@)",
)

pkg_tar(
    name = "debian_etc_nginx",
    srcs = [
        "{nginx}:config_includes",
    ],
    mode = "0644",
    package_dir = "/etc/nginx",
)

pkg_tar(
    name = "debian_usr_share_nginx_html",
    srcs = [
        "{nginx}:html_files",
    ],
    mode = "0644",
    package_dir = "/usr/share/nginx/html",
)

pkg_tar(
    name = "debian_var",
    srcs = [
        "var/cache/nginx/.empty",
        "var/log/nginx/.empty",
    ],
    mode = "0644",
    strip_prefix = ".",
)
"""

_PKGOSS_BUILD_FILE_TAIL = """
pkg_tar(
    name = "debian_overlay",
    srcs = [
        "etc/default/nginx",
        "etc/init.d/nginx",
        "etc/logrotate.d/nginx",
        "etc/nginx/conf.d/default.conf",
        "etc/nginx/nginx.conf",
        "usr/share/man/man8/nginx.8",
    ],
    mode = "0644",
    modes = {
        "etc/init.d/nginx": "0755",
    },
    strip_prefix = ".",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":debian_etc_nginx",
        ":debian_usr_share_nginx_html",
        ":debian_var",
    ],
)
"""

_ZLIB_BUILD_FILE = """
# Copyright (C) 2015-2018 Google Inc.
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

licenses(["notice"])

exports_files(["README"])

cc_library(
    name = "zlib",
    srcs = [
        "adler32.c",
        "crc32.c",
        "crc32.h",
        "deflate.c",
        "deflate.h",
        "infback.c",
        "inffast.c",
        "inffast.h",
        "inffixed.h",
        "inflate.c",
        "inflate.h",
        "inftrees.c",
        "inftrees.h",
        "trees.c",
        "trees.h",
        "zconf.h",
        "zutil.c",
        "zutil.h",
    ],
    hdrs = [
        "zlib.h",
    ],
    defines = [
        "Z_SOLO",
    ],
    visibility = [
        "//visibility:public",
    ],
)
"""

def nginx_repositories_boringssl(bind):
    git_repository(
        name = "boringssl",
        commit = "5b8bd1ba221804c81c8a92c6d1d353ef43a851ab",  # 2018-12-14
        remote = "https://boringssl.googlesource.com/boringssl",
    )

    if bind:
        native.bind(
            name = "boringssl_crypto",
            actual = "@boringssl//:crypto",
        )

        native.bind(
            name = "boringssl_ssl",
            actual = "@boringssl//:ssl",
        )

def nginx_repositories_brotli(bind):
    git_repository(
        name = "org_brotli",
        commit = "222564a95d9ab58865a096b8d9f7324ea5f2e03e",  # 2016-12-02
        remote = "https://github.com/google/brotli.git",
    )

    if bind:
        native.bind(
            name = "brotli_enc",
            actual = "@org_brotli//:brotlienc",
        )

        native.bind(
            name = "brotli_dec",
            actual = "@org_brotli//:brotlidec",
        )

def nginx_repositories_ngx_brotli(nginx):
    new_git_repository(
        name = "ngx_brotli",
        build_file_content = _NGX_BROTLI_BUILD_FILE.format(nginx = nginx),
        commit = "5ead1ada782b18c7b38a3c2798a40a334801c7b6",  # 2016-12-05
        remote = "https://nginx.googlesource.com/ngx_brotli",
    )

def nginx_repositories_pcre(bind):
    http_archive(
        name = "nginx_pcre",
        build_file_content = _PCRE_BUILD_FILE,
        sha256 = "69acbc2fbdefb955d42a4c606dfde800c2885711d2979e356c0636efde9ec3b5",
        strip_prefix = "pcre-8.42",
#        url = "https://ftp.pcre.org/pub/pcre/pcre-8.42.tar.gz",
        url = "https://sourceforge.net/projects/pcre/files/pcre/8.42/pcre-8.42.tar.gz",
    )

    if bind:
        native.bind(
            name = "pcre",
            actual = "@nginx_pcre//:pcre",
        )

def nginx_repositories_pkgoss(nginx):
    new_git_repository(
        name = "nginx_pkgoss",
        build_file_content = _PKGOSS_BUILD_FILE.format(nginx = nginx) +
                             _PKGOSS_BUILD_FILE_TAIL,
        commit = "2456bf617acaa11b06c11481082797909b300f45",  # nginx-1.15.8
        remote = "https://nginx.googlesource.com/nginx-pkgoss",
    )

def nginx_repositories_zlib(bind):
    new_git_repository(
        name = "nginx_zlib",
        build_file_content = _ZLIB_BUILD_FILE,
        commit = "cacf7f1d4e3d44d871b605da3b647f07d718623f",  # v1.2.11
        remote = "https://github.com/madler/zlib.git",
    )

    if bind:
        native.bind(
            name = "zlib",
            actual = "@nginx_zlib//:zlib",
        )

def nginx_repositories(bind = False, nginx = "@nginx//", ngx_brotli = False):
    # core dependencies
    nginx_repositories_boringssl(bind)
    nginx_repositories_pcre(bind)
    nginx_repositories_zlib(bind)

    # packaging
    nginx_repositories_pkgoss(nginx)

    # ngx_brotli + dependencies
    if ngx_brotli:
        nginx_repositories_ngx_brotli(nginx)
        nginx_repositories_brotli(bind)
