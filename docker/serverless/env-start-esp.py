#!/usr/bin/env python
#
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

import os

CMD = "/usr/sbin/start_esp"

# The command being run must be the 0th arg.
ARGS = [CMD, "--enable_backend_routing"]

# A set of ESP flags supported for ESP Serverless
SUPPORTED_ESP_FLAGS = set([
  "--cors_allow_credentials",
  "--cors_allow_headers",
  "--cors_allow_methods",
  "--cors_allow_origin_regex",
  "--cors_expose_headers",
  "--enable_debug",
  "--service_control_url_override"
])

def assert_env_var(name):
    if name not in os.environ:
        raise ApplicationError(
            "Serverless ESP expects {} in environment variables.".format(name)
        )

def extract_flags(esp_args):
    flags = set()
    for arg in esp_args:
        arg = arg.strip()
        if not arg.startswith("-"):
            continue

        flag = arg
        if "=" in arg:
            flag = arg[:arg.index("=")].strip()

        for i, c in enumerate(arg):
            if c.isspace():
                flag = arg[:i]
                break

        flags.add(flag)

    return flags

def verify_esp_args(esp_args):
  provided_flags = extract_flags(esp_args)
  if not provided_flags.issubset(SUPPORTED_ESP_FLAGS):
      unsupported_flags = provided_flags - SUPPORTED_ESP_FLAGS
      raise ApplicationError(
          "Serverless ESP does not support the following flags:\n {}".format(
              "  \n".join(unsupported_flags))
      )


assert_env_var("PORT")
ARGS.append("--http_port={}".format(os.environ["PORT"]))

assert_env_var("ENDPOINTS_SERVICE_NAME")
ARGS.append("--service={}".format(os.environ["ENDPOINTS_SERVICE_NAME"]))

if "ENDPOINTS_SERVICE_VERSION" in os.environ:
    ARGS.extend(
        [
            "--rollout_strategy=fixed",
            "--version={}".format(os.environ["ENDPOINTS_SERVICE_VERSION"]),
        ]
    )
else:
    ARGS.append("--rollout_strategy=managed")

if "CORS_PRESET" in os.environ:
    ARGS.append("--cors_preset={}".format(os.environ["CORS_PRESET"]))

if "ESP_ARGS" in os.environ:
    # By default, ESP_ARGS is comma-separated.
    # But if a comma needs to appear within an arg, there is an alternative
    # syntax: Pick a replacement delimiter, specify it at the beginning of the
    # string between two caret (^) symbols, and use it within the arg string.
    # Example:
    # ^++^--cors_allow_methods="GET,POST,PUT,OPTIONS"++--cors_allow_credentials
    arg_value = os.environ["ESP_ARGS"]

    delim = ","
    if arg_value.startswith("^") and "^" in arg_value[1:]:
        delim, arg_value = arg_value[1:].split("^", 1)
    if not delim:
        raise ApplicationError("Malformed ESP_ARGS environment variable.")

    esp_args = arg_value.split(delim)
    verify_esp_args(esp_args)
    ARGS.extend(esp_args)


os.execv(CMD, ARGS)
