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
load("//:nginx.bzl", "nginx_suite")

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
    tests = glob([
            "nginx-tests/*.t",
            "http_latency_status.t",
        ],
        exclude = [
            # ssl.t, ssl_certificates.t, ssl_stapling are excluded from the
            # test suite because boring ssl does not support renegotiation
            # feature required by the test.
            "nginx-tests/h2_ssl_verify_client.t",
            "nginx-tests/ssl.t",
            "nginx-tests/ssl_certificates.t",
            "nginx-tests/ssl_stapling.t",
            "nginx-tests/ssl_certificate_chain.t",
            "nginx-tests/ssl_client_escaped_cert.t",
            "nginx-tests/ssl_crl.t",
            "nginx-tests/ssl_verify_client.t",
            "nginx-tests/ssl_verify_depth.t",
            "nginx-tests/stream_proxy_protocol_ssl.t",
            "nginx-tests/stream_ssl_verify_client.t",
            # Jenkins tests require root so env(TEST_NGINX_GLOBALS) adds
            # "user root;" in nginx.bzl. But this test also adds "user root;"
            # so it fails. Just skip it for now.
            "nginx-tests/proxy_bind_transparent.t",
            # Not sure why this one fails
            "nginx-tests/proxy_variables.t",
        ],
    ),
    deps = [
        ":nginx_test",
    ],
)
