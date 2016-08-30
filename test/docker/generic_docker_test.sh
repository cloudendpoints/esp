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
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Load error handling utilities
. ${ROOT}/script/all-utilities || { echo "Cannot load Bash utilities" ; exit 1 ; }

DEB=""

while getopts d: arg; do
  case ${arg} in
    d) DEB=${OPTARG};;
    ?) exit 1;;
  esac
done

export ACCESS_TOKEN=$(uuidgen)

[[ -f "${DEB}" ]] || error_exit "Debian package \"${DEB}\" doesn't exist."
TEMP_DIR="$(command mktemp -d)"
cp ${DEB} ${TEMP_DIR}/endpoints-runtime.deb
cp ${ROOT}/docker/generic/* ${TEMP_DIR}
cp ${ROOT}/test/docker/backend/service.json ${ROOT}/test/docker/management

function wait_for() {
  local URL=${1}

  for (( I=0 ; I<60 ; I++ )); do
    printf "\nWaiting for ${URL}\n"
    curl --silent ${URL} && return 0
    sleep 1
  done
  return 1
}

# Build Docker images.
docker build -t esp-image ${TEMP_DIR} \
  || error_exit "Cannot build the Endpoints Proxy Docker image."

export METADATA_PORT=7000
export APP_PORT=7001
export CONTROL_PORT=7002
export MANAGEMENT_PORT=7003

for service in metadata control management; do
  pushd $DIR/$service
  npm install
  nodejs $service.js &
  popd
done

pushd $DIR/backend
npm install
nodejs bookstore.js &
popd

trap 'jobs -p | xargs kill' EXIT

wait_for "localhost:${METADATA_PORT}/computeMetadata/v1/instance/service-accounts/default/token" \
  || error_exit "Metadata container failed to start."
wait_for "localhost:${APP_PORT}/shelves" \
  || error_exit "App failed to start."
wait_for "localhost:${CONTROL_PORT}/" \
  || error_exit "Service control failed to start."
wait_for "localhost:${MANAGEMENT_PORT}/" \
  || error_exit "Service management failed to start."

printf "\nCalling metadata.\n"
curl -v localhost:${METADATA_PORT}/computeMetadata/v1/instance/service-accounts/default/token
METADATA_RESULT=$?

printf "\nCalling app.\n"
curl -v localhost:${APP_PORT}/shelves
APP_RESULT=$?

printf "\nCalling control :check and :report.\n"
curl -v -d="" localhost:${CONTROL_PORT}/v1/services/bookstore-backend.endpointsv2.appspot.com:check \
  && curl -v -d="" localhost:${CONTROL_PORT}/v1/services/bookstore-backend.endpointsv2.appspot.com:report
CONTROL_RESULT=$?

printf "\nCalling management.\n"
curl -v localhost:${MANAGEMENT_PORT}
MANAGEMENT_RESULT=$?

# The service name and version come from backend/README.md
printf "\n -- Run docker test for http requests.\n"
${DIR}/run_generic_docker_test.sh \
  -p 8080 \
  -a localhost:${APP_PORT} \
  -s bookstore-backend.endpointsv2.appspot.com \
  -v 2016-04-25R1 ||
  error_exit "Docker test for http requests failed."

printf "\n -- Run docker test for https requests.\n"
${DIR}/run_generic_docker_test.sh \
  -p 8080 \
  -a localhost:${APP_PORT} -S 443 \
  -s bookstore-backend.endpointsv2.appspot.com \
  -v 2016-04-25R1 ||
  error_exit "Docker test for https requests failed."

printf "\n -- Run docker test for custom nginx.conf.\n"
${DIR}/run_generic_docker_test.sh \
  -n /etc/nginx/custom/nginx.conf \
  -s bookstore-backend.endpointsv2.appspot.com \
  -v 2016-04-25R1 ||
  error_exit "Docker test for custom nginx.conf failed."

printf "\n -- Run docker test for status port change.\n"
${DIR}/run_generic_docker_test.sh \
  -p 8080 \
  -a localhost:${APP_PORT} -N 9050 \
  -s bookstore-backend.endpointsv2.appspot.com \
  -v 2016-04-25R1 ||
  error_exit "Docker test for custom nginx.conf failed."

printf "\n -- Run docker test for port change.\n"
${DIR}/run_generic_docker_test.sh \
  -p 9050 \
  -a localhost:${APP_PORT} \
  -s bookstore-backend.endpointsv2.appspot.com \
  -v 2016-04-25R1 ||
  error_exit "Docker test for custom nginx.conf failed."

printf "\nShutting down.\n"
docker rmi esp-image

echo "Metadata result: ${METADATA_RESULT}"
echo "App result: ${APP_RESULT}"
echo "Control result: ${CONTROL_RESULT}"
echo "Management result: ${MANAGEMENT_RESULT}"

[[ ${METADATA_RESULT} -eq 0 ]] \
  && [[ ${APP_RESULT} -eq 0 ]] \
  && [[ ${CONTROL_RESULT} -eq 0 ]] \
  && [[ ${MANAGEMENT_RESULT} -eq 0 ]] \
  || error_exit "Test failed."
