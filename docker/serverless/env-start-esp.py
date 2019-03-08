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

from __future__ import print_function
import os


def ensure_dir(dirname):
    if not os.path.exists(dirname):
        try:
            os.makedirs(dirname)
        except OSError as exc:
            print("[ESP] Cannot create directory.")
            sys.exit(3)


def assert_env_var(name):
    if name not in os.environ:
        raise KeyError(
            "Serverless ESP expects {} in environment variables.".format(name)
        )


def make_error_app(error_msg):
    # error_msg must be a utf-8 or ascii bytestring
    def error_app(environ, start_response):
        start_response("503 Service Unavailable", [("Content-Type", "text/plain")])
        return [error_msg, "\n"]

    return error_app


def serve_error_msg(error_msg):
    print("Serving error handler with '{}'.".format(error_msg))
    import wsgiref.simple_server

    app = make_error_app(error_msg)
    port = int(os.environ["PORT"])
    server = wsgiref.simple_server.make_server("", port, app)
    server.serve_forever()


def main():
    # nginx needs these directories to exist, but knative may mount volumes in
    # /var, hiding the directories created in the Dockerfile.
    for dirname in ["/var/log/nginx", "/var/cache/nginx"]:
        ensure_dir(dirname)

    CMD = "/usr/sbin/start_esp"

    # The command being run must be the 0th arg.
    ARGS = [CMD, "--enable_backend_routing"]

    # Uncaught KeyError; if no port, we can't serve a nice error handler. Crash instead.
    assert_env_var("PORT")
    ARGS.append("--http_port={}".format(os.environ["PORT"]))

    try:
        assert_env_var("ENDPOINTS_SERVICE_NAME")
    except KeyError as error:
        serve_error_msg(error.message)
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
            serve_error_msg("Malformed ESP_ARGS environment variable.")

        ARGS.extend(arg_value.split(delim))

    os.execv(CMD, ARGS)


if __name__ == "__main__":
    main()
