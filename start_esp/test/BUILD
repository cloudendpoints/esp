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

genrule(
    name = "import_nginx_conf_template",
    srcs = ["//start_esp:nginx-auto.conf.template"],
    outs = ["nginx-conf-template"],
    cmd = "cat $(location //start_esp:nginx-auto.conf.template) > $@",
)

genrule(
    name = "import_server_conf_template",
    srcs = ["//start_esp:server-auto.conf.template"],
    outs = ["server-conf-template"],
    cmd = "cat $(location //start_esp:server-auto.conf.template) > $@",
)

py_binary(
    name = "start_esp_binary",
    srcs = [
        "//start_esp:fetch_service_config.py",
        "//start_esp:start_esp.py",
    ],
    main = "//start_esp:start_esp.py",
)

py_test(
    name = "start_esp_test",
    srcs = [
        "start_esp_test.py",
    ],
    data = [
        ":nginx-conf-template",
        ":server-conf-template",
        ":start_esp_binary",
        "//src/tools:read_server_config",
    ] + glob(["testdata/*"]),
)
