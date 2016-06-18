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

ACCESS_TOKEN=$(uuidgen)

[[ -f "${DEB}" ]] || error_exit "Debian package \"${DEB}\" doesn't exist."
TEMP_DIR="$(command mktemp -d)"
cp ${DEB} ${TEMP_DIR}/endpoints-runtime.deb
cp ${ROOT}/docker/generic/* ${TEMP_DIR}
cp -f ${ROOT}/test/docker/esp_generic/nginx.conf.template ${TEMP_DIR}

# Build Docker images.
docker build -t metadata-image ${ROOT}/test/docker/metadata \
  || error_exit "Cannot build a fake metadata Docker image."
docker build -t app-image ${ROOT}/test/docker/backend \
  || error_exit "Cannot build the backend Docker image."
docker build -t control-image ${ROOT}/test/docker/control \
  || error_exit "Cannot build the fake service control Docker image."
docker build -t esp-image ${TEMP_DIR} \
  || error_exit "Cannot build the Endpoints Proxy Docker image."

# Start metadata container.
docker run \
    --name=metadata \
    --detach=true \
    --publish-all \
    --env="ACCESS_TOKEN=${ACCESS_TOKEN}" \
    metadata-image \
  || error_exit "Cannot start metadata container."

# Start app container.
docker run \
    --name=app \
    --detach=true \
    --publish-all \
    app-image \
  || error_exit "Cannot start app container."

# Start service control container.
docker run \
    --name=control \
    --detach=true \
    --publish-all \
    --env="ACCESS_TOKEN=${ACCESS_TOKEN}" \
    control-image \
  || error_exit "Cannot start service control container."

METADATA_PORT=$(docker port metadata 8080) \
  || error_exit "Cannot get metadata port number."
APP_PORT=$(docker port app 8080) \
  || error_exit "Cannot get app port number."
CONTROL_PORT=$(docker port control 8080) \
  || error_exit "Cannot get service control port number."

if [[ "$(uname)" == "Darwin" ]]; then
  IP=$(docker-machine ip default)
  METADATA_PORT=${IP}:${METADATA_PORT##*:}
  APP_PORT=${IP}:${APP_PORT##*:}
  CONTROL_PORT=${IP}:${CONTROL_PORT##*:}
fi

function wait_for() {
  local URL=${1}

  for (( I=0 ; I<60 ; I++ )); do
    printf "\nWaiting for ${URL}\n"
    curl --silent ${URL} && return 0
    sleep 1
  done
  return 1
}

wait_for "${METADATA_PORT}/computeMetadata/v1/instance/service-accounts/default/token" \
  || error_exit "Metadata container failed to start."
wait_for "${APP_PORT}/shelves" \
  || error_exit "App failed to start."
wait_for "${CONTROL_PORT}/" \
  || error_exit "Service control failed to start."

printf "\nCalling metadata.\n"
curl -v ${METADATA_PORT}/computeMetadata/v1/instance/service-accounts/default/token
METADATA_RESULT=$?

printf "\nCalling app.\n"
curl -v ${APP_PORT}/shelves
APP_RESULT=$?

printf "\nCalling control :check and :report.\n"
curl -v -d="" ${CONTROL_PORT}/v1/services/bookstore-backend.endpointsv2.appspot.com:check \
  && curl -v -d="" ${CONTROL_PORT}/v1/services/bookstore-backend.endpointsv2.appspot.com:report
CONTROL_RESULT=$?

# The service name and version come from backend/README.md
printf "\nRun docker test for http requests.\n"
${DIR}/run_generic_docker_test.sh \
  -a app:8080 \
  -s bookstore-backend.endpointsv2.appspot.com \
  -v 2016-04-25R1 ||
  error_exit "Docker test for http requests failed."

printf "\nRun docker test for https requests.\n"
${DIR}/run_generic_docker_test.sh \
  -a app:8080 -S 443 \
  -s bookstore-backend.endpointsv2.appspot.com \
  -v 2016-04-25R1 ||
  error_exit "Docker test for https requests failed."

printf "\nRun docker test for custom nginx.conf.\n"
${DIR}/run_generic_docker_test.sh \
  -n /etc/nginx/custom/nginx.conf \
  -s bookstore-backend.endpointsv2.appspot.com \
  -v 2016-04-25R1 ||
  error_exit "Docker test for custom nginx.conf failed."

printf "\nRun docker test for status port change.\n"
${DIR}/run_generic_docker_test.sh \
  -a app:8080 -N 9000 \
  -s bookstore-backend.endpointsv2.appspot.com \
  -v 2016-04-25R1 ||
  error_exit "Docker test for custom nginx.conf failed."

printf "\nShutting down.\n"
docker stop metadata app control
docker rm metadata app control
docker rmi metadata-image app-image control-image esp-image

echo "Metadata result: ${METADATA_RESULT}"
echo "App result: ${APP_RESULT}"
echo "Control result: ${CONTROL_RESULT}"

[[ ${METADATA_RESULT} -eq 0 ]] \
  && [[ ${APP_RESULT} -eq 0 ]] \
  && [[ ${CONTROL_RESULT} -eq 0 ]] \
  || error_exit "Test failed."
