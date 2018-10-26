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

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. ${DIR}/jenkins-utilities || { echo "Cannot load Bash utilities" ; exit 1 ; }

function usage() {
  [[ -n "${1}" ]] && echo "${1}"
  cat <<EOF
usage: ${BASH_SOURCE[0]}
  -a ...  delete services
  -d ...  days old
  -f ...  no prompt
  -i ...  delete instances
  -l ...  skip activate service account step in linux-gae-instance
  -n ...  delete namespaces
  -p ...  project
  -r ...  regex
  -s ...  git commit sha
  -v ...  delete AppEngine version
  -z ...  zone
EOF
  exit 1
}

# Deleting filtered esp instances
function delete_instances() {
  local instances=("${@}")
  [[ -z "${instances}" ]] \
    && { echo 'No GCE instance to delete'; return; }
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
  [[ -z "${namespaces}" ]] \
    && { echo 'No GKE namespace to delete'; return; }
  for namespace in "${namespaces[@]}"; do
    echo "Deleting namespaces: ${namespace}"
    user_input \
      && run "${KUBECTL}" delete namespace ${namespace}
  done
}

# Deleting filtered esp services
function delete_services() {
  local services=("${@}")
  [[ -z "${services}" ]] \
    && { echo 'No API service to delete'; return; }
  for service in "${services[@]}"; do
    echo "Deleting services: ${service}"
    user_input \
      && run delete_service ${service}
  done
}

# Deleting filtered AppEngine Versions
function delete_versions() {
  local versions=("${@}")
  [[ -z "${versions}" ]] \
    && { echo 'No AppEngine version to delete'; return; }
  local extra_args=()
  [[ ${SET_SERVICE_ACCOUNT} == false ]] \
    && extra_args+=(-l )

  for version in "${versions[@]}"; do
    echo "Deleting AppEngine version: ${version}"
    user_input \
      && run "${DIR}/linux-gae-instance" \
          -v "${version}" \
          -p "${PROJECT}" \
          "${extra_args[@]}" \
          delete
  done
}

function parse_gcloud_json() {
  local json_path="${1}"
  local json_key="${2}"
  local key_regex="${3}"

  python - << __EOF__ "${json_path}" "${json_key}" "${key_regex}"
import json
import re
import sys

regex_str = '.*'

if len(sys.argv) != 4 :
  exit (1)

json_file = sys.argv[1]
json_key = sys.argv[2]
regex_str = sys.argv[3]
regex = re.compile(r'%s' % regex_str)
filtered_items = []

if None in [json_file, json_key, regex_str]:
  exit(1)

with open(json_file, 'r') as f:
  items = json.load(f)

for item in items:
  value = item.get(json_key, None)
  if value and regex.match(value):
    filtered_items.append(value)

for value in filtered_items:
  print value

exit(0 if filtered_items else 1)
__EOF__
  local status=${?}
  rm -f "${json_path}"
  [[ ${status} -eq 0 ]] && return 0
  return 1
}

# List esp instances
function list_instances() {
  local regex=${1}
  local json_path="$(mktemp /tmp/XXXXX.json)"
  "${GCLOUD}" compute instances list \
    --project "${PROJECT}" \
    --zones "${ZONE}" \
    --format=json > "${json_path}"
  parse_gcloud_json "${json_path}" 'name' "${regex}"
  rm -f "${json_path}"
}

# List existing esp Kubernetes namespaces
function list_namespaces() {
  local regex=${1}
  local namespaces="$("${KUBECTL}" get namespaces \
    -o=custom-columns=NAME:.metadata.name \
    | grep ${regex})"
  [[ -n "${namespaces}" ]] && { echo "${namespaces}"; return 0; }
  return 1
}

# List existing esp services
function list_services() {
  local regex=${1}
  local json_path="$(mktemp /tmp/XXXXX.json)"
  "${GCLOUD}" services list --available \
    --project "${PROJECT}" \
    --produced \
    --format=json > "${json_path}"
  parse_gcloud_json "${json_path}" 'serviceName' "${regex}"
  rm -f "${json_path}"
}

# List Flex versions
function list_versions() {
  local regex=${1}
  local json_path="$(mktemp /tmp/XXXXX.json)"
  "${GCLOUD}" app versions list \
    --project "${PROJECT}" \
    --format=json > "${json_path}"
  parse_gcloud_json "${json_path}" 'id' "${regex}"
  rm -f "${json_path}"
}

function filter_results() {
  local results=("${@}")
  [[ -z "${results}" ]] && return

  for result in "${results[@]}"; do
    if [[ -n "${DAYS_OLD}" ]]; then
      # Extracting date from result
      local creation_date="${result: -10:6}"
      if [[ -n "${creation_date}" ]]; then
        # Date validation
        date -d "${creation_date}" +'%y%m%d' &> /dev/null || continue
        # Getting expiration date in the same format
        local expiration_date="$(date +'%y%m%d' -d "${DAYS_OLD} days ago")"
        # Hack to compare date
        [[ ${expiration_date#0} -ge  ${creation_date#0} ]] \
          && echo "${result}"
      fi
    else
       echo "${result}"
    fi
  done
}

function user_input() {
  [[ ${INTERACTIVE} == false ]] && return 0
  local reply=''
  read -r -p 'Are you sure? ' reply
  [[ "${reply}" =~ ^[Yy]$ ]] && return 0
  return 1
}

DAYS_OLD=''
DELETE_INSTANCES=false
DELETE_NAMESPACES=false
DELETE_SERVICES=false
DELETE_VERSIONS=false
DEFAULT_FILTER='test-'
GCLOUD="$(which gcloud)" || GCLOUD="${HOME}/google-cloud-sdk/bin/gcloud"
INTERACTIVE=true
KUBECTL="$(which kubectl)" || KUBECTL="${HOME}/google-cloud-sdk/bin/kubectl"
PROJECT='endpoints-jenkins'
REGEX=''
SET_SERVICE_ACCOUNT=true
SHA=''
ZONE='us-central1-f'

while getopts :ad:filnp:r:s:vz: arg; do
  case ${arg} in
    a) DELETE_SERVICES=true;;
    d) DAYS_OLD="${OPTARG}";;
    f) INTERACTIVE=false;;
    i) DELETE_INSTANCES=true;;
    l) SET_SERVICE_ACCOUNT=false;;
    n) DELETE_NAMESPACES=true;;
    p) PROJECT="${OPTARG}";;
    r) REGEX="${OPTARG}";;
    s) SHA="${OPTARG}";;
    v) DELETE_VERSIONS=true;;
    z) ZONE="${OPTARG}";;
    *) usage "Invalid option: -${OPTARG}";;
  esac
done

[[ -n "${DAYS_OLD}" ]] \
  || [[ -n "${REGEX}" ]]\
  || [[ -n "${SHA}" ]] \
  || usage "Should specify at least an option"

# We use only the first 7 ascii from the SHA
if [[ -z "${REGEX}" ]]; then
  REGEX=".*${DEFAULT_FILTER}${SHA:0:7}.*"
fi

if [[ ${DELETE_INSTANCES} == true ]]; then
  instances=($(list_instances ${REGEX}))
  filtered_instances=($(filter_results "${instances[@]}"))
  delete_instances "${filtered_instances[@]}"
fi

if [[ ${DELETE_NAMESPACES} == true ]]; then
  namespaces=($(list_namespaces ${REGEX}))
  filtered_namespaces=($(filter_results "${namespaces[@]}"))
  delete_namespaces "${filtered_namespaces[@]}"
fi

if [[ ${DELETE_SERVICES} == true ]]; then
  services=($(list_services ${REGEX}))
  filtered_services=($(filter_results "${services[@]}"))
  delete_services "${filtered_services[@]}"
fi

if [[ ${DELETE_VERSIONS} == true ]]; then
  versions=($(list_versions ${REGEX}))
  filtered_versions=($(filter_results "${versions[@]}"))
  delete_versions "${filtered_versions[@]}"
fi
