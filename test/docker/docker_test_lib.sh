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

DIR="${ROOT}/test/docker/esp_generic"
DOCKER_IMAGES=()
DOCKER_CONTAINERS=()
LINKS=()

function docker_ip() {
  local ip="$(docker inspect --format '{{ .NetworkSettings.IPAddress }}' "$@")"
  [[ -z "${ip}" ]] && return 1
  echo "${ip}"
  return 0
}

function docker_build() {
  local tag="${1}"
  local path="${2}"
  retry -n 2 docker build --rm=true -t "${tag}" "${path}" \
    || error_exit "Cannot build ${tag}"
  DOCKER_IMAGES+=("${tag}")
}

function wait_for() {
  local url=${1}

  for (( i=0 ; i<60 ; i++ )); do
    printf "\nWaiting for ${url}\n"
    curl --silent ${url} && return 0
    sleep 1
  done
  return 1
}

function esp_test() {
  local target_address=${1}
  local shelves_result
  local shelves_body
  local books_result
  local books_body

  curl -v -k ${target_address}/shelves
  shelves_result=$?
  shelves_body=$(curl --silent -k ${target_address}/shelves)

  curl -v -k ${target_address}/shelves/1/books
  books_result=$?
  books_body=$(curl --silent -k ${target_address}/shelves/1/books)

  echo "shelves result: ${shelves_result}"
  echo "books result: ${books_result}"

  [[ "${shelves_body}" == *"\"Fiction\""* ]] \
    || error_exit "/shelves did not return Fiction: ${shelves_body}"
  [[ "${shelves_body}" == *"\"Fantasy\""* ]] \
    || error_exit "/shelves did not return Fantasy: ${shelves_body}"
  error_message="/shelves/1/books did not return unregistered callers"
  [[ "${books_body}" == *"Method doesn't allow unregistered callers"* ]] \
    || error_exit "$error_message: ${books_result}"

  [[ ${shelves_result} -eq 0 ]] \
    && [[ ${books_result} -eq 0 ]] \
    || error_exit "Test failed."
}

function get_port() {
  local container_id="${1}"
  local port=${2}
  local url=''
  local url=$(docker port "${container_id}" ${port}) \
    || return 1
  if [[ "$(uname)" == 'Darwin' ]]; then
    local ip="$(docker-machine ip default)"
    url="${ip}:${url##*:}"
  fi
  echo "${url}" && return 0
  return 1
}

function run_esp_test() {
  local OPTIND OPTARG arg
  local port=8080
  local port2=''
  local ssl_port=''
  local nginx_status_port=8090
  local nginx_conf_path=''
  local docker_args=('--detach=true' '--publish-all' '--expose=8080')
  local esp_args=()
  local esp_image=''
  local container_id=''

  while getopts 'a:e:n:N:p:P:S:s:v:c:m:k:' arg; do
    case ${arg} in
      a) esp_args+=(-a "${OPTARG}");;
      e) esp_image="${OPTARG}";;
      n)
        nginx_conf_path="${OPTARG}"
        esp_args+=(-n "${nginx_conf_path}");
        docker_args+=("--volume=${DIR}/custom_nginx.conf:${nginx_conf_path}");
        ;;
      N)
        nginx_status_port="${OPTARG}"
        esp_args+=(-N "${nginx_status_port}")
        docker_args+=("--expose=${nginx_status_port}")
        ;;
      p)
        port="${OPTARG}"
        esp_args+=(-p "${port}" )
        docker_args+=("--expose=${port}")
        ;;
      P)
        port2="${OPTARG}"
        esp_args+=(-P "${port2}")
        docker_args+=("--expose=${port2}")
        ;;
      S)
        ssl_port="${OPTARG}"
        esp_args+=(-S "${ssl_port}")
        docker_args+=("--expose=${ssl_port}" \
          "--volume=${DIR}/nginx.key:/etc/nginx/ssl/nginx.key" \
          "--volume=${DIR}/nginx.crt:/etc/nginx/ssl/nginx.crt")
        ;;
      s) esp_args+=(-s "${OPTARG}");;
      v) esp_args+=(-v "${OPTARG}");;
      c) esp_args+=(-c "${OPTARG}");;
      m) esp_args+=(-m "${OPTARG}");;
      k) esp_args+=(-k);;
    esac
  done

  #
  for l in ${LINKS[@]}; do docker_args+=(--link="${l}"); done

  echo "Running command:"
  echo "docker run ${docker_args[@]} "${esp_image}" ${esp_args[@]}"
  # Start Endpoints proxy container.
  container_id="$(docker run ${docker_args[@]} "${esp_image}" ${esp_args[@]})" \
    || error_exit 'Cannot start Endpoints proxy container.'
  echo "ESP container is ${container_id}"
  DOCKER_CONTAINERS+=("${container_id}")

  esp_http_port="$(get_port "${container_id}" ${port})" \
    || error_exit 'Cannot get esp port'
  esp_nginx_status_port="$(get_port "${container_id}" ${nginx_status_port})" \
    || error_exit 'Cannot get esp_nginx_status_port'

  printf "\nCheck esp status port.\n"
  wait_for "${esp_nginx_status_port}/nginx_status" \
    || error_exit "esp container didn't come up."

  printf "\nStart testing esp http requests.\n"
  esp_test "http://${esp_http_port}"

  if [[ "${ssl_port}" ]]; then
    esp_ssl_port="$(get_port "${container_id}" ${ssl_port})" \
      || error_exit 'Cannot get esp ssl_port'
    printf "\nStart testing esp https requests.\n"
    esp_test "https://${esp_ssl_port}"
  fi
}

