#!/bin/bash
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

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# Load error handling utilities
. ${ROOT}/script/all-utilities || { echo "Cannot load Bash utilities" ; exit 1 ; }

PORT=''
SSL_PORT=''
NGINX_STATUS_PORT=''
SERVER_ADDRESS=''
NGINX_CONF_PATH=''
PUBLISH='--publish-all'
SERVICE_NAME=''
SERVICE_VERSION=''

while getopts 'a:n:N:p:S:s:v:' arg; do
  case ${arg} in
    a) SERVER_ADDRESS="${OPTARG}";;
    s) SERVICE_NAME="${OPTARG}";;
    v) SERVICE_VERSION="${OPTARG}";;
    n) NGINX_CONF_PATH="${OPTARG}";;
    N) NGINX_STATUS_PORT="${OPTARG}";;
    p) PORT="${OPTARG}";;
    S) SSL_PORT="${OPTARG}";;
  esac
done

if [[ -z "${SERVICE_NAME}" ]]; then
  echo "-s SERVICE_NAME must be provided!"
  exit 2
fi

if [[ -z "${SERVICE_VERSION}" ]]; then
  echo "-v SERVICE_VERSION must be provided!"
  exit 2
fi

ARGS=()
DIR="${ROOT}/test/docker/esp_generic"
ARGS+=(-m http://127.0.0.1:${METADATA_PORT})
ARGS+=(-c "http://127.0.0.1:${MANAGEMENT_PORT}/service_config")
ARGS+=(-s "${SERVICE_NAME}")
ARGS+=(-v "${SERVICE_VERSION}")
[[ -n "${SERVER_ADDRESS}" ]] && ARGS+=(-a "${SERVER_ADDRESS}")

if [[ -n "${PORT}" ]]; then
  ARGS+=(-p "${PORT}")
else
  PORT=8080
fi

[[ -n "${SSL_PORT}" ]]          && {
  ARGS+=(-S "${SSL_PORT}");
  PUBLISH+=" --expose=${SSL_PORT}";
  VOLUMES+=" --volume=${DIR}/nginx.key:/etc/nginx/ssl/nginx.key";
  VOLUMES+=" --volume=${DIR}/nginx.crt:/etc/nginx/ssl/nginx.crt";
}
[[ -n "${NGINX_STATUS_PORT}" ]] && {
  ARGS+=(-N "${NGINX_STATUS_PORT}");
  PUBLISH+=" --expose=${NGINX_STATUS_PORT}";
}
[[ -n "${NGINX_CONF_PATH}" ]]   && {
  ARGS+=(-n "${NGINX_CONF_PATH}");
  VOLUMES+=" --volume=${DIR}/custom_nginx.conf:${NGINX_CONF_PATH}";
}

# Start Endpoints proxy container.
docker run \
    --net="host" \
    --name=esp \
    --detach=true \
    ${PUBLISH} \
    ${VOLUMES} \
    esp-image \
    ${ARGS[@]} \
  || error_exit "Cannot start Endpoints proxy container."

function wait_for() {
  local URL=${1}

  for (( I=0 ; I<60 ; I++ )); do
    printf "\nWaiting for ${URL}\n"
    curl --silent ${URL} && return 0
    sleep 1
  done
  return 1
}

function start_esp_test() {
  local TARGET_ADDRESS=${1}

  curl -v -k ${TARGET_ADDRESS}/shelves
  SHELVES_RESULT=$?
  SHELVES_BODY=$(curl --silent -k ${TARGET_ADDRESS}/shelves)

  curl -v -k ${TARGET_ADDRESS}/shelves/1/books
  BOOKS_RESULT=$?
  BOOKS_BODY=$(curl --silent -k ${TARGET_ADDRESS}/shelves/1/books)

  echo "Shelves result: ${SHELVES_RESULT}"
  echo "Books result: ${BOOKS_RESULT}"

  [[ "${SHELVES_BODY}" == *"\"Fiction\""* ]] \
    || error_exit "/shelves did not return Fiction: ${SHELVES_BODY}"
  [[ "${SHELVES_BODY}" == *"\"Fantasy\""* ]] \
    || error_exit "/shelves did not return Fantasy: ${SHELVES_BODY}"
  ERROR_MESSAGE="/shelves/1/books did not return unregistered callers"
  [[ "${BOOKS_BODY}" == *"Method doesn't allow unregistered callers"* ]] \
    || error_exit "$ERROR_MESSAGE: ${BOOKS_RESULT}"

  [[ ${SHELVES_RESULT} -eq 0 ]] \
    && [[ ${BOOKS_RESULT} -eq 0 ]] \
    || error_exit "Test failed."
}

# Default nginx status port is 8090.
[[ -n "${NGINX_STATUS_PORT}" ]] || NGINX_STATUS_PORT=8090

printf "\nCheck esp status port.\n"
wait_for "localhost:${NGINX_STATUS_PORT}/nginx_status" \
  || error_exit "ESP container didn't come up."

printf "\nStart testing esp http requests.\n"
start_esp_test "localhost:${PORT}"

if [[ "${SSL_PORT}" ]]; then
  printf "\nStart testing esp https requests.\n"
  start_esp_test "https://localhost:${SSL_PORT}"
fi

UUID="$(uuidgen)"
ELOG=~/error-${UUID}.log
ALOG=~/access-${UUID}.log
docker cp esp:/var/log/nginx/error.log "${ELOG}"
docker cp esp:/var/log/nginx/access.log "${ALOG}"
echo "Logs saved into ${ELOG}, ${ALOG}"
printf "\nNGINX error log:\n"
cat ${ELOG}

printf "\n\nShutting down esp.\n"
docker stop esp
docker rm esp
