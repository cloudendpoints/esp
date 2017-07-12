#!/bin/bash
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
################################################################################
#
# Script to generate auth token based on `src/tools/auth_token_gen`

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
AUTH_TOKEN_GEN="${ROOT}/bazel-bin/src/tools/auth_token_gen"
BUILD_AUTH_TOKEN_GEN=1

. ${ROOT}/script/all-utilities || { echo 'Cannot load Bash utilities'; exit 1; }

# By default audience is service name,  use -a to change it to your service
# name or other allowed audiences (check service swagger configuration).
AUDIENCE="bookstore-esp-echo.cloudendpointsapis.com"

function usage() {
  echo "usage: $0 [options ...]"
  echo "options:"
  echo "  -s <secret file>"
  echo "  -a <audience>"
  echo "  -g <path to auth_token_gen file>"
  exit 2
}

while getopts a:g:s:? arg; do
  case ${arg} in
    a) AUDIENCE=${OPTARG};;
    g)
       AUTH_TOKEN_GEN=${OPTARG}
       BUILD_AUTH_TOKEN_GEN=0
       ;;
    s) SECRET_FILE=${OPTARG};;
    ?) usage;;
  esac
done

# By default, use jwk key. Can be switched to x509 or symmetric key.
SECRET_FILE="${SECRET_FILE:-$(get_test_client_key ${ROOT}/client/custom/esp-test-client-secret-jwk.json)}"

if [[ ! -x ${AUTH_TOKEN_GEN} ]]; then
  [[ ${BUILD_AUTH_TOKEN_GEN} -ne 0 ]] \
    || error_exit "Cannot find ${AUTH_TOKEN_GEN}"

  ${BAZEL} ${BAZEL_ARGS} build ${BAZEL_BUILD_ARGS} \
    //src/tools:auth_token_gen \
    || error_exit "Failed to build //src/tools:auth_token_gen"
fi

${AUTH_TOKEN_GEN} ${SECRET_FILE} ${AUDIENCE} \
  | grep 'Auth token' \
  | awk -F': ' '{print $2}'
