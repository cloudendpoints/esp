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

DEB=""

while getopts d: arg; do
  case ${arg} in
    d) DEB=${OPTARG};;
    ?) exit 1;;
  esac
done

ACCESS_TOKEN=$(uuidgen)

[[ -f "${DEB}" ]] || error_exit "Debian package \"${DEB}\" doesn't exist."
cp "${DEB}" "${ROOT}/test/docker/esp/endpoints-runtime.deb"

# Build Docker images.
docker build -t metadata-image ${ROOT}/test/docker/metadata \
  || error_exit "Cannot build a fake metadata Docker image."
docker build -t gaeapp-image ${ROOT}/test/docker/backend \
  || error_exit "Cannot build the backend Docker image."
docker build -t control-image ${ROOT}/test/docker/control \
  || error_exit "Cannot build the fake service control Docker image."
docker build -t esp-image ${ROOT}/test/docker/esp \
  || error_exit "Cannot build the Endpoints Proxy Docker image."

# Start metadata container.
docker run \
    --name=metadata \
    --detach=true \
    --publish-all \
    --env="ACCESS_TOKEN=${ACCESS_TOKEN}" \
    metadata-image \
  || error_exit "Cannot start metadata container."

# Start gaeapp container.
docker run \
    --name=gaeapp \
    --detach=true \
    --publish-all \
    gaeapp-image \
  || error_exit "Cannot start gaeapp container."

# Start service control container.
docker run \
    --name=control \
    --detach=true \
    --publish-all \
    --env="ACCESS_TOKEN=${ACCESS_TOKEN}" \
    control-image \
  || error_exit "Cannot start service control container."

# Start Endpoints proxy container.
docker run \
    --name=esp \
    --detach=true \
    --publish-all \
    --link=metadata:metadata \
    --link=control:control \
    --link=gaeapp:gaeapp \
    --volumes-from=gaeapp \
    esp-image \
  || error_exit "Cannot start Endpoints proxy container."

METADATA_PORT=$(docker port metadata 8080) \
  || error_exit "Cannot get metadata port number."
GAEAPP_PORT=$(docker port gaeapp 8080) \
  || error_exit "Cannot get gaeapp port number."
CONTROL_PORT=$(docker port control 8080) \
  || error_exit "Cannot get service control port number."
ESP_PORT=$(docker port esp 8080) \
  || error_exit "Cannot get esp port number."
ESP_NGINX_STATUS_PORT=$(docker port esp 8090) \
  || error_exit "Cannot get esp status port number."

if [[ "$(uname)" == "Darwin" ]]; then
  IP=$(docker-machine ip default)
  METADATA_PORT=${IP}:${METADATA_PORT##*:}
  GAEAPP_PORT=${IP}:${GAEAPP_PORT##*:}
  CONTROL_PORT=${IP}:${CONTROL_PORT##*:}
  ESP_PORT=${IP}:${ESP_PORT##*:}
  ESP_NGINX_STATUS_PORT=${IP}:${ESP_NGINX_STATUS_PORT##*:}
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
wait_for "${GAEAPP_PORT}/shelves" \
  || error_exit "Gaeapp failed to start."
wait_for "${CONTROL_PORT}/" \
  || error_exit "Service control failed to start."
wait_for "${ESP_NGINX_STATUS_PORT}/nginx_status" \
  || error_exit "Endpoints Proxy failed to start."

printf "\nCalling metadata.\n"
curl -v ${METADATA_PORT}/computeMetadata/v1/instance/service-accounts/default/token
METADATA_RESULT=$?

printf "\nCalling gaeapp.\n"
curl -v ${GAEAPP_PORT}/shelves
GAEAPP_RESULT=$?

printf "\nCalling control :check and :report.\n"
curl -v -d="" ${CONTROL_PORT}/v1/services/bookstore-backend.endpointsv2.appspot.com:check \
  && curl -v -d="" ${CONTROL_PORT}/v1/services/bookstore-backend.endpointsv2.appspot.com:report
CONTROL_RESULT=$?

printf "\nCalling esp.\n"
curl -v ${ESP_PORT}/shelves
SHELVES_RESULT=$?
SHELVES_BODY=$(curl --silent ${ESP_PORT}/shelves)

curl -v ${ESP_PORT}/shelves/1/books
BOOKS_RESULT=$?
BOOKS_BODY=$(curl --silent ${ESP_PORT}/shelves/1/books)

printf "\nGetting NGINX logs.\n"
echo '*************************************************************************'
docker exec esp cat /var/log/nginx/access.log
docker exec esp cat /var/log/nginx/error.log
echo '*************************************************************************'

printf "\nShutting down.\n"
docker stop metadata gaeapp control esp
docker rm metadata gaeapp control esp
docker rmi metadata-image gaeapp-image control-image esp-image

echo "Metadata result: ${METADATA_RESULT}"
echo "Gaeapp result: ${GAEAPP_RESULT}"
echo "Control result: ${CONTROL_RESULT}"
echo "Shelves result: ${SHELVES_RESULT}"
echo "Books result: ${BOOKS_RESULT}"

[[ "${SHELVES_BODY}" == *"\"Fiction\""* ]] \
  || error_exit "/shelves did not return Fiction: ${SHELVES_BODY}"
[[ "${SHELVES_BODY}" == *"\"Fantasy\""* ]] \
  || error_exit "/shelves did not return Fantasy: ${SHELVES_BODY}"
[[ "${BOOKS_BODY}" == *"Method doesn't allow unregistered callers"* ]] \
  || error_exit "/shelves/1/books did not return unregistered callers: ${BOOKS_RESULT}"

[[ ${METADATA_RESULT} -eq 0 ]] \
  && [[ ${GAEAPP_RESULT} -eq 0 ]] \
  && [[ ${CONTROL_RESULT} -eq 0 ]] \
  && [[ ${SHELVES_RESULT} -eq 0 ]] \
  && [[ ${BOOKS_RESULT} -eq 0 ]] \
  || error_exit "Test failed."
