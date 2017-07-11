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
load("@io_bazel_rules_perl//perl:perl.bzl", "perl_library")
load("/nginx", "nginx_suite")

perl_library(
    name = "nginx_test",
    srcs = glob([
        "nginx-tests/lib/Test/**/*.pm",
    ]),
    visibility = ["//visibility:public"],
)

# Nginx test suite containing all of the integration tests
# included with Nginx.


nginx_suite(
    nginx = "//src/nginx/main:nginx-esp",
    tags = ["exclusive"],
    tests = glob(
        ["nginx-tests/*.t"],
        exclude = [
            # limit_req.t is excluded from the test suite because we use 429
            # instead of 503 as response status code
            "nginx-tests/limit_req.t",
            # ssl2.t is excluded from the test suite because boring ssl
            # does not support SSL_get0_raw_cipherlist used by $ssl_ciphers
            # variable required by the test.
            "nginx-tests/ssl2.t",
            # ssl_sni_reneg.t is excluded from the test suite because boring ssl
            # does not support renegotiation feature required by the test.
            "nginx-tests/ssl_sni_reneg.t",
        ],
    ),
    deps = [
        ":nginx_test",
    ],
)
