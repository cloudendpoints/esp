#!/bin/bash
#
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

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. ${DIR}/jenkins-utilities || { echo "Cannot load Bash utilities" ; exit 1 ; }

function usage() {
  [[ -n "${1}" ]] && echo "${1}"
  cat <<EOF
usage: ${BASH_SOURCE[0]}
  -d <days old>
  -n <no prompt>
  -r <regex>
  -s <git commit SHA>
  -p <project>
  -z <zone>
EOF
  exit 1
}

# Deleting filtered esp instances
function delete_instances() {
  local instances=("${@}")
  [[ -z "${instances}" ]] && return

  for instance in "${instances[@]}"; do
    echo "Deleting instances: ${instance}"
    user_input \
      && run "${GCLOUD}" compute instances delete \
          --project "${PROJECT}" \
          --zone "${ZONE}" \
          "${instance}" -q
  done
}

# Deleting filtered esp kubernetes namespaces
function delete_namespaces() {
  local namespaces=("${@}")
  [[ -z "${namespaces}" ]] && return

  for namespace in "${namespaces[@]}"; do
    echo "Deleting namespaces: ${namespace}"
    user_input \
      && run "${KUBECTL}" delete namespace ${namespace}
  done
}

# Deleting filtered esp services
function delete_services() {
  local services=("${@}")
  [[ -z "${services}" ]] && return

  for service in "${services[@]}"; do
    echo "Deleting services: ${service}"
    user_input \
      && run delete_service ${service}
  done
}

# List esp instances
function list_instances() {
  local instances="$("${GCLOUD}" compute instances list \
        --project "${PROJECT}" \
        --zone "${ZONE}" \
    | grep 'test-' \
    | cut -d ' ' -f 1)"
  [[ -n "${instances}" ]] && echo "${instances}"
}

# List existing esp Kubernetes namespaces
function list_namespaces() {
  local namespaces="$("${KUBECTL}" get namespaces \
    | grep 'test-' \
    | cut -d ' ' -f 1)"
  [[ -n "${namespaces}" ]] && echo "${namespaces}"
}

# List existing esp services
function list_services() {
  local services="$(run "${GCLOUD}" alpha service-management list \
      --project "${PROJECT}" \
      --produced \
    | grep 'test-' \
    | cut -d ' ' -f 1)"
  [[ -n "${services}" ]] && echo "${services}"
}


function filter_results() {
  local results=("${@}")
  local prefix='test'
  [[ -z "${results}" ]] && return

  # We use only the first 7 ascii from the SHA
  if [[ -n "${SHA}" ]]; then
    prefix="test-${SHA:0:7}"
  fi

  for result in "${results[@]}"; do
    local keep='false'
    if [[ "${result}" =~ "${prefix}" ]]; then
      keep='true'
    else
      if [[ -n "${REGEX}" ]]; then
        [[ "${result}" =~ ${REGEX} ]] && keep='true'
      fi
    fi
    if [[ "${keep}"  == 'true' ]]; then
      if [[ -n "${DAYS_OLD}" ]]; then
        # Extracting date from result
        local creation_date="${result: -10:6}"
        if [[ -n "${creation_date}" ]]; then
          # Getting expiration date in the same format
          local expiration_date="$(date +'%y%m%d' -d "${DAYS_OLD} days ago")"
          # Hack to compare date
          [[ ${expiration_date#0} -ge  ${creation_date#0} ]] || keep='false'
         fi
      fi
    fi

    [[ "${keep}"  == 'true' ]] && echo "${result}"
  done
}

function user_input() {
  [[ "${INTERACTIVE}" == 'false' ]] && return 0
  local reply=''
  read -r -p 'Are you sure? ' reply
  [[ "${reply}" =~ ^[Yy]$ ]] && return 0
  return 1
}

DAYS_OLD=''
INTERACTIVE='true'
REGEX=''
SHA=''
PROJECT='endpoints-jenkins'
ZONE='us-central1-f'
GCLOUD="$(which gcloud)" || GCLOUD="${HOME}/google-cloud-sdk/bin/gcloud"
KUBECTL="$(which kubectl)" || KUBECTL="${HOME}/google-cloud-sdk/bin/kubectl"

while getopts :d:nr:p:s:z: arg; do
  case ${arg} in
    d) DAYS_OLD="${OPTARG}";;
    n) INTERACTIVE='false';;
    r) REGEX="${OPTARG}";;
    p) PROJECT="${OPTARG}";;
    s) SHA="${OPTARG}";;
    z) ZONE="${OPTARG}";;
    *) usage "Invalid option: -${OPTARG}";;
  esac
done

[[ -n "${DAYS_OLD}" ]] \
  || [[ -n "${REGEX}" ]]\
  || [[ -n "${SHA}" ]] \
  || usage "Should specify at least an option"

instances=($(list_instances))
filtered_instances=($(filter_results "${instances[@]}"))
delete_instances "${filtered_instances[@]}"

namespaces=($(list_namespaces))
filtered_namespaces=($(filter_results "${namespaces[@]}"))
delete_namespaces "${filtered_namespaces[@]}"

services=($(list_services))
filtered_services=($(filter_results "${services[@]}"))
delete_services "${filtered_services[@]}"
