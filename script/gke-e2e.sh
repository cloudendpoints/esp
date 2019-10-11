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

SCRIPT_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ESP_ROOT="$(cd "${SCRIPT_PATH}/../" && pwd)"

. ${ESP_ROOT}/script/jenkins-utilities || { echo "Cannot load Jenkins Bash utilities" ; exit 1 ; }

function cleanup {
  if [[ "${SKIP_CLEANUP}" == 'false' ]]; then
    run kubectl delete namespace "${NAMESPACE}"
    # Uncomment this line when the limit on #services is lifted or increased to > 20
    # run gcloud endpoints services delete ${ESP_SERVICE} --quiet
  fi
}

function get_service_ip () {
  local ns="${1}"
  local name="${2}"
  local COUNT=10
  local SLEEP=15
  for i in $( seq 1 ${COUNT} ); do
    local host=$(kubectl -n "${ns}" get service "${name}" | awk '{print $4}' | grep -v EXTERNAL-IP)
      [ '<pending>' != $host ] && break
      echo "Waiting for server external ip. Attempt  #$i/${COUNT}... will try again in ${SLEEP} seconds" >&2
      sleep ${SLEEP}
  done
  if [[ '<pending>' == $host ]]; then
    echo 'Failed to get the GKE cluster host.'
    return 1
  else
    echo "$host"
    return 0
  fi
}

e2e_options "${@}"

if [[ "${BACKEND}" == 'bookstore' ]]; then
  YAML_FILE_TEMP="${ESP_ROOT}/test/bookstore/gke/deploy_secure_template.yaml"
  SERVICE_IDL="${ESP_ROOT}/test/bookstore/swagger_template.json"
  # GKE service name in the deploy.yaml
  GKE_SERVICE_NAME="bookstore"
  TEST_ID_SUFFIX="secure"
  CREATE_SERVICE_ARGS="${SERVICE_IDL}"
elif [[ "${BACKEND}" == 'interop' ]]; then
  YAML_FILE_TEMP="${ESP_ROOT}/test/grpc/gke/interop.yaml.temp"
  SERVICE_IDL="${ESP_ROOT}/test/grpc/grpc-interop.yaml"
  # GKE service name in the deploy.yaml
  GKE_SERVICE_NAME="interop"
  TEST_ID_SUFFIX="grpc-ssl"
  CREATE_SERVICE_ARGS="${SERVICE_IDL} bazel-genfiles/test/grpc/grpc-interop.descriptor"
fi

TEST_ID="gke-${COUPLING_OPTION}-${TEST_TYPE}-${BACKEND}-${TEST_ID_SUFFIX}"
ESP_SERVICE="${TEST_ID}.${PROJECT_ID}.appspot.com"
NAMESPACE="${UNIQUE_ID}"

sed -i "s|\${ENDPOINT_SERVICE}|${ESP_SERVICE}|g" "${SERVICE_IDL}"
# Deploy new service config
create_service "${ESP_SERVICE}" "${CREATE_SERVICE_ARGS}"

YAML_FILE="$(mktemp /tmp/yaml.XXXX)"
sed -e "s|\$BACKEND_IMAGE|${BOOKSTORE_IMAGE}|g" \
    -e "s|\$ESP_IMAGE|${ESP_IMAGE}|g" \
    -e "s|\${ENDPOINT_SERVICE}|${ESP_SERVICE}|g" \
  ${YAML_FILE_TEMP} > ${YAML_FILE}
cat ${YAML_FILE}

trap cleanup EXIT

# Testing protocol
run kubectl create namespace "${NAMESPACE}" || error_exit "Namespace already exists"

if [[ "${BACKEND}" == 'interop' ]]; then
  run kubectl -n "${NAMESPACE}" create secret generic grpc-ssl \
    --from-file=${ESP_ROOT}/src/nginx/t/testdata/grpc/cacert.pem \
    --from-file=${ESP_ROOT}/src/nginx/t/testdata/grpc/servercert.pem \
    --from-file=${ESP_ROOT}/src/nginx/t/testdata/grpc/serverkey.pem
fi

run gsutil cp gs://esp-testing-secret-files/endpoints-jenkins-ea2e9cc046a6.json \
    /tmp/service_account_key.json
run kubectl -n "${NAMESPACE}" create secret generic sa-file \
    --from-file=/tmp/service_account_key.json

run kubectl create -f ${YAML_FILE}          --namespace "${NAMESPACE}"
run kubectl get services -o yaml            --namespace "${NAMESPACE}"
run kubectl get deployments -o yaml         --namespace "${NAMESPACE}"

SERVICE_IP=$(get_service_ip "${NAMESPACE}" "${GKE_SERVICE_NAME}")
if [[ "${BACKEND}" == 'bookstore' ]]; then
  HOST="http://${SERVICE_IP}"
elif [[ "${BACKEND}" == 'interop' ]]; then
  HOST="${SERVICE_IP}:8080"
fi
echo "=== Use the host: ${HOST}";

LOG_DIR="$(mktemp -d /tmp/log.XXXX)"

# Running Test
run_nonfatal long_running_test \
  "${HOST}" \
  "${DURATION_IN_HOUR}" \
  "${API_KEY}" \
  "${ESP_SERVICE}" \
  "${LOG_DIR}" \
  "${TEST_ID}" \
  "${UNIQUE_ID}"
STATUS=${?}

if [[ -n $REMOTE_LOG_DIR ]]; then
  upload_logs "${REMOTE_LOG_DIR}" "${LOG_DIR}"
  rm -rf "${LOG_DIR}"
fi

exit ${STATUS}
