# Copyright 2017 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_tar", "pkg_deb")
load("@io_bazel_rules_pex//pex:pex_rules.bzl", "pex_binary")

package(default_visibility = ["//visibility:public"])

pkg_tar(
    name = "esp-bin",
    files = [
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
        "oauth2client>=3.0.0",
        "pyasn1>=0.1.9",
        "pyasn1-modules>=0.0.8",
        "urllib3>=1.16",
    ],
)

pkg_tar(
    name = "nginx-conf-template",
    files = [
        ":nginx-auto.conf.template",
    ],
    mode = "0644",
    package_dir = "/etc/nginx",
    strip_prefix = ".",
)

pkg_tar(
    name = "server-conf-template",
    files = [
        ":server-auto.conf.template",
        ":server_config.pb.txt",
    ],
    mode = "0644",
    package_dir = "/etc/nginx",
    strip_prefix = ".",
)

pkg_tar(
    name = "trusted-ca-certificates",
    files = [
        ":trusted-ca-certificates.crt",
    ],
    mode = "0644",
    package_dir = "/etc/nginx",
    strip_prefix = ".",
)

pkg_tar(
    name = "nginx-initd-override",
    files = [
        ":nginx-init.d/nginx",
    ],
    mode = "0755",
    package_dir = "/etc/init.d",
)
