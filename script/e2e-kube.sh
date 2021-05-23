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
YAML_TEMP_FILE=${ESP_ROOT}/test/bookstore/backend.yaml_temp
YAML_FILE=${ESP_ROOT}/test/bookstore/backend.yaml

. ${ESP_ROOT}/script/jenkins-utilities || { echo "Cannot load Jenkins Bash utilities" ; exit 1 ; }

set -x

e2e_options "${@}"

TEST_ID="gke-${COUPLING_OPTION}-${TEST_TYPE}-${BACKEND}"
if [[ "${ESP_ROLLOUT_STRATEGY}" == "managed" ]]; then
  TEST_ID="${TEST_ID}-managed"
fi

# Remove this line when the limit on #services is lifted or increased to > 20
ESP_SERVICE="${TEST_ID}.${PROJECT_ID}.appspot.com"

NAMESPACE="${UNIQUE_ID}"
ARGS="\"--status_port=8090\", \"--access_log=off\", \"--service=${ESP_SERVICE}\""

case "${BACKEND}" in
  'bookstore' )
    SERVICE_IDL="${ESP_ROOT}/test/bookstore/swagger_template.json"
    CREATE_SERVICE_ARGS="${SERVICE_IDL}"
    ARGS="$ARGS, \"--backend=127.0.0.1:8081\"";;
  'echo'      )
    SERVICE_IDL="${ESP_ROOT}/test/grpc/grpc-test.yaml"
    CREATE_SERVICE_ARGS="${SERVICE_IDL} ${ESP_ROOT}/bazel-genfiles/test/grpc/grpc-test.descriptor"
    ARGS="$ARGS, \"--backend=grpc://127.0.0.1:8081\"";;
  'interop'   )
    SERVICE_IDL="${ESP_ROOT}/test/grpc/grpc-interop.yaml"
    CREATE_SERVICE_ARGS="${SERVICE_IDL} ${ESP_ROOT}/bazel-genfiles/test/grpc/grpc-interop.descriptor"
    ARGS="$ARGS, \"--backend=grpc://127.0.0.1:8081\"";;
  *           ) e2e_usage "Invalid backend option";;
esac

run sed -i "s|\${ENDPOINT_SERVICE}|${ESP_SERVICE}|g" ${SERVICE_IDL}
create_service "${ESP_SERVICE}" "${CREATE_SERVICE_ARGS}"
ARGS="$ARGS, \"--version=${ESP_SERVICE_VERSION}\", \"--rollout_strategy=${ESP_ROLLOUT_STRATEGY}\""

case "${TEST_TYPE}" in
  'http'  ) ARGS="$ARGS, \"--http_port=80\"";;
  'http2' ) ARGS="$ARGS, \"--http2_port=80\", \"--http_port=8080\"";;
  'https' ) ARGS="$ARGS, \"--ssl_port=443\"";;
  *       ) e2e_usage "Invalid test type";;
esac

if [[ "${COUPLING_OPTION}" == 'custom' ]]; then
    ARGS="$ARGS, \"-n=/etc/nginx/ssl/nginx.conf\""
fi

sed -e "s|BACKEND_IMAGE|${BOOKSTORE_IMAGE}|g" \
    -e "s|ESP_IMAGE|${ESP_IMAGE}|g" \
    -e "s|ESP_ARGS|${ARGS}|g" ${YAML_TEMP_FILE} | tee ${YAML_FILE}

trap gke_namespace_cleanup EXIT

# Testing protocol
run kubectl create namespace "${NAMESPACE}" || error_exit "Namespace already exists"

run kubectl create secret generic esp-ssl \
    --from-file=${ESP_ROOT}/tools/src/testdata/nginx.crt \
    --from-file=${ESP_ROOT}/tools/src/testdata/nginx.key \
    --from-file=${ESP_ROOT}/test/bookstore/gke/nginx.conf  --namespace "${NAMESPACE}"

run kubectl create --validate=false -f ${YAML_FILE}          --namespace "${NAMESPACE}"

SERVICE_IP=$(get_gke_service_ip "${NAMESPACE}" "bookstore")
case "${TEST_TYPE}" in
  'http'  ) HOST="http://${SERVICE_IP}";;
  'http2' ) HOST="${SERVICE_IP}:80";;
  'https' ) HOST="https://${SERVICE_IP}";;
  *       ) e2e_usage "Invalid test type";;
esac

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

# Deploy new config and check new rollout on /endpoints_status
if [[ ("${ESP_ROLLOUT_STRATEGY}" == "managed") && ("${BACKEND}" == "bookstore") ]] ; then
  # Deploy new service config
  create_service "${ESP_SERVICE}" "${SERVICE_IDL}"

  # Need to wait for ServiceControl to detect new rollout
  # Here wait for 200 seconds.
  for l in {1..20}
  do
    echo "Wait for the new config to propagate: ${l}"
    sleep 10
  done

  run retry -n 10 wait_for_service_config_rollouts_update "gke" "${HOST}:8090/endpoints_status" "$ESP_SERVICE_VERSION 100" \
    || error_exit 'Rollouts update was failed'
fi

run kubectl logs $(kubectl get pods -n ${NAMESPACE} --no-headers|awk '{print $1}') -c esp -n ${NAMESPACE} \
  | tee ${LOG_DIR}/error.log

if [[ -n $REMOTE_LOG_DIR ]]; then
  upload_logs "${REMOTE_LOG_DIR}" "${LOG_DIR}"
  rm -rf "${LOG_DIR}"
fi

exit ${STATUS}
