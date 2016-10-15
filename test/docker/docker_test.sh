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
. "${ROOT}/script/all-utilities" || { echo 'Cannot load Bash utilities' ; exit 1 ; }
. "${ROOT}/test/docker/docker_test_lib.sh" || error_exit 'Cannot load docker test lib'

DEB=''
SKIP_CLEANUP=false
TAG="$(git rev-parse --short HEAD)-$(date +"%Y%m%d%H%M%S")"

while getopts d:st: arg; do
  case "${arg}" in
    d) DEB="${OPTARG}";;
    s) SKIP_CLEANUP=true;;
    t) TAG="${OPTARG}";;
    ?) error_exit 'Unknown flag.';;
  esac
done

[[ -f "${DEB}" ]] || error_exit "Debian package \"${DEB}\" doesn't exist."

ACCESS_TOKEN="$(uuidgen)"
TEMP_DIR="$(command mktemp -d)"

cp -f "${DEB}" "${ROOT}/test/docker/esp/endpoints-runtime.deb"
cp -f ${DEB} ${TEMP_DIR}/endpoints-runtime.deb
cp -f ${ROOT}/docker/generic/* ${TEMP_DIR}
cp -f ${ROOT}/test/docker/backend/service.json ${ROOT}/test/docker/management

METADATA_IMAGE="metadata-image:${TAG}"
APP_IMAGE="app-image:${TAG}"
CONTROL_IMAGE="control-image:${TAG}"
MANAGEMENT_IMAGE="management-image:${TAG}"
ESP_IMAGE="esp-image:${TAG}"


function cleanup {
  printf "\nPrinting container logs.\n"
  for c in ${DOCKER_CONTAINERS[@]}; do
    echo "Logs for container ${c}:"
    docker logs "${c}"
  done
  [[ ${SKIP_CLEANUP}  == true ]] && return
  printf "\nShutting down.\n"
  [[ -n "${DOCKER_CONTAINERS}" ]] && docker stop ${DOCKER_CONTAINERS[@]}
  [[ -n "${DOCKER_CONTAINERS}" ]] && docker rm -f ${DOCKER_CONTAINERS[@]}
  [[ -n "${DOCKER_IMAGES}" ]] && docker rmi -f ${DOCKER_IMAGES[@]}
}

trap cleanup EXIT

# Build Docker images.
docker_build "${METADATA_IMAGE}" "${ROOT}/test/docker/metadata"
docker_build "${APP_IMAGE}" "${ROOT}/test/docker/backend"
docker_build "${CONTROL_IMAGE}" "${ROOT}/test/docker/control"
docker_build "${MANAGEMENT_IMAGE}" "${ROOT}/test/docker/management"
docker_build "${ESP_IMAGE}" "${TEMP_DIR}"

# nginx does not resolve local name in proxy_pass therefore we have to find the
# ip address in the docker bridge network to allow container to talk to each
# others.
# Start metadata container.
METADATA_CONTAINER="$(docker run \
  --detach=true \
  --publish-all \
  --env="ACCESS_TOKEN=${ACCESS_TOKEN}" \
  "${METADATA_IMAGE}")" \
  || error_exit 'Cannot start metadata container.'
DOCKER_CONTAINERS+=("${METADATA_CONTAINER}")
LINKS+=("${METADATA_CONTAINER}:metadata")
METADATA_DOCKER_IP="$(docker_ip "${METADATA_CONTAINER}")" \
  || error_exit 'Cannot find metadata service ip.'
echo "Metadata Server ip is ${METADATA_DOCKER_IP}."

# Start app container.
APP_CONTAINER="$(docker run \
  --detach=true \
  --publish-all \
  "${APP_IMAGE}")" \
  || error_exit 'Cannot start app container.'
DOCKER_CONTAINERS+=("${APP_CONTAINER}")
LINKS+=("${APP_CONTAINER}:app")
APP_DOCKER_IP="$(docker_ip "${APP_CONTAINER}")" \
  || error_exit 'Cannot find app server ip.'
echo "App Server ip is ${APP_DOCKER_IP}."

# Start service control container.
CONTROL_CONTAINER="$(docker run \
  --detach=true \
  --publish-all \
  --env="ACCESS_TOKEN=${ACCESS_TOKEN}" \
  "${CONTROL_IMAGE}")" \
  || error_exit 'Cannot start service control container.'
DOCKER_CONTAINERS+=("${CONTROL_CONTAINER}")
LINKS+=("${CONTROL_CONTAINER}:control")
CONTROL_DOCKER_IP="$(docker_ip "${CONTROL_CONTAINER}")" \
  || error_exit 'Cannot find service control ip.'
echo "Service Control ip is ${CONTROL_DOCKER_IP}."

MANAGEMENT_CONTAINER="$(docker run \
  --detach=true \
  --publish-all \
  --env="ACCESS_TOKEN=${ACCESS_TOKEN}" \
  --env="CONTROL_URL=http://${CONTROL_DOCKER_IP}:8080" \
  "${MANAGEMENT_IMAGE}")" \
  || error_exit 'Cannot start management container.'
DOCKER_CONTAINERS+=("${MANAGEMENT_CONTAINER}")
LINKS+=("${MANAGEMENT_CONTAINER}:management")
MANAGEMENT_DOCKER_IP="$(docker_ip "${MANAGEMENT_CONTAINER}")" \
  || error_exit 'Cannot find management server ip.'
echo "Service Management ip is ${MANAGEMENT_DOCKER_IP}."

METADATA_PORT=$(get_port "${METADATA_CONTAINER}" 8080) \
  || error_exit 'Cannot get metadata port number.'
APP_PORT=$(get_port "${APP_CONTAINER}" 8080) \
  || error_exit 'Cannot get app port number.'
CONTROL_PORT=$(get_port "${CONTROL_CONTAINER}" 8080) \
  || error_exit 'Cannot get service control port number.'
MANAGEMENT_PORT=$(get_port "${MANAGEMENT_CONTAINER}" 8080) \
  || error_exit 'Cannot get management port number.'

wait_for "${METADATA_PORT}/computeMetadata/v1/instance/service-accounts/default/token" \
  || error_exit 'Metadata container failed to start.'
wait_for "${APP_PORT}/shelves" \
  || error_exit 'Gaeapp failed to start.'
wait_for "${CONTROL_PORT}/" \
  || error_exit 'Service control failed to start.'
wait_for "${MANAGEMENT_PORT}/" \
  || error_exit 'Management failed to start.'

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

printf "\nCalling management.\n"
curl -v ${MANAGEMENT_PORT}
MANAGEMENT_RESULT=$?

echo '*************************************************************************'
echo "Metadata result: ${METADATA_RESULT}"
echo "App result: ${APP_RESULT}"
echo "Control result: ${CONTROL_RESULT}"
echo "Management result: ${MANAGEMENT_RESULT}"

[[ ${METADATA_RESULT} -eq 0 ]] \
  && [[ ${APP_RESULT} -eq 0 ]] \
  && [[ ${CONTROL_RESULT} -eq 0 ]] \
  && [[ ${MANAGEMENT_RESULT} -eq 0 ]] \
  || error_exit 'Test failed.'

# The service name and version come from backend/README.md
# We need to provide metadata for secret
# Default port is 8080
printf "\nRun docker test for http requests.\n"
run_esp_test \
  -a "${APP_DOCKER_IP}:8080" \
  -e "${ESP_IMAGE}" \
  -m "http://${METADATA_DOCKER_IP}:8080" \
  -c "http://${MANAGEMENT_DOCKER_IP}:8080/service_config" \
  -s bookstore-backend.endpointsv2.appspot.com \
  -v 2016-04-25R1 \
  || error_exit 'Docker test for http requests failed.'

# Service name, version, and token are fetched from metadata
printf "\nRun docker test for https requests.\n"
run_esp_test \
  -e "${ESP_IMAGE}" \
  -p 8080 \
  -S 8443 \
  -m "http://${METADATA_DOCKER_IP}:8080" \
  -c "http://${MANAGEMENT_DOCKER_IP}:8080/service_config" \
  -a "${APP_DOCKER_IP}:8080" \
  || error_exit 'Docker test for https requests failed.'

# Still need to fetch service.json for custom nginx
printf "\nRun docker test for custom nginx.conf.\n"
run_esp_test \
  -e "${ESP_IMAGE}" \
  -n /etc/nginx/custom/nginx.conf \
  -m "http://${METADATA_DOCKER_IP}:8080" \
  -c "http://${MANAGEMENT_DOCKER_IP}:8080/service_config" \
  -s bookstore-backend.endpointsv2.appspot.com \
  -v 2016-04-25R1 \
  || error_exit 'Docker test for custom nginx.conf failed.'

printf "\nRun docker test for custom ports.\n"
run_esp_test \
  -e "${ESP_IMAGE}" \
  -p 9000 \
  -P 9001 \
  -N 9050 \
  -m "http://${METADATA_DOCKER_IP}:8080" \
  -c "http://${MANAGEMENT_DOCKER_IP}:8080/service_config" \
  -a "${APP_DOCKER_IP}:8080" \
  -s bookstore-backend.endpointsv2.appspot.com \
  -v 2016-04-25R1 \
  || error_exit 'Docker test for custom ports failed.'

