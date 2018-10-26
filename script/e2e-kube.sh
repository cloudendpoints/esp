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
CLI="$ESP_ROOT/espcli"
YAML_FILE=${ESP_ROOT}/test/bookstore/bookstore.yaml
ESP_APP="esp"

. ${ESP_ROOT}/script/jenkins-utilities || { echo "Cannot load Jenkins Bash utilities" ; exit 1 ; }

# Fetch CLI tool if it is not available
if [[ ! -f $CLI ]]; then
  OSNAME=`uname | tr "[:upper:]" "[:lower:]"`
  URL="https://storage.googleapis.com/endpoints-release/v1.0.4/bin/${OSNAME}/amd64/espcli"
  curl -o ${CLI} ${URL}
  chmod +x ${CLI}
fi

function cleanup {
  if [[ "${SKIP_CLEANUP}" == 'false' ]]; then
    run kubectl delete namespace "${NAMESPACE}"
    # Uncomment this line when the limit on #services is lifted or increased to > 20
    # run gcloud endpoints services delete ${ESP_SERVICE} --quiet
  fi
}

e2e_options "${@}"

TEST_ID="gke-${COUPLING_OPTION}-${TEST_TYPE}-${BACKEND}"

# Remove this line when the limit on #services is lifted or increased to > 20
ESP_SERVICE="${TEST_ID}.${PROJECT_ID}.appspot.com"

NAMESPACE="${UNIQUE_ID}"
ARGS="\
  -N 8090 \
  --access_log off \
  --service ${ESP_SERVICE} \
  --project ${PROJECT_ID} \
  --image ${ESP_IMAGE} \
  --sslCert ${ESP_ROOT}/tools/src/testdata/nginx.crt \
  --sslKey ${ESP_ROOT}/tools/src/testdata/nginx.key \
"
run sed_i "s|image:.*|image: ${BOOKSTORE_IMAGE}|g" ${YAML_FILE}
run cat ${YAML_FILE}

case "${BACKEND}" in
  'bookstore' )
    SERVICE_IDL="${ESP_ROOT}/test/bookstore/swagger_template.json";;
  'echo'      )
    SERVICE_IDL="${ESP_ROOT}/test/grpc/grpc-test.yaml"
    ARGS="$ARGS -g --config ${ESP_ROOT}/bazel-genfiles/test/grpc/grpc-test.descriptor";;
  'interop'   )
    SERVICE_IDL="${ESP_ROOT}/test/grpc/grpc-interop.yaml"
    ARGS="$ARGS -g --config ${ESP_ROOT}/bazel-genfiles/test/grpc/grpc-interop.descriptor";;
  *           ) e2e_usage "Invalid backend option";;
esac
run sed -i "s|\${ENDPOINT_SERVICE}|${ESP_SERVICE}|g" ${SERVICE_IDL}
ARGS="$ARGS --config ${SERVICE_IDL}"

case "${COUPLING_OPTION}" in
  'loose' ) ARGS="$ARGS -d loose";;
  'tight' ) ARGS="$ARGS -d tight";;
  'custom') ARGS="$ARGS -d tight -n ${ESP_ROOT}/test/bookstore/gke/nginx.conf";;
  *       ) e2e_usage "Invalid test option";;
esac

HOST="${ESP_APP}.${NAMESPACE}.svc.cluster.local"
case "${TEST_TYPE}" in
  'http'  ) ARGS="$ARGS -p 80";         HOST="http://${HOST}";;
  'http2' ) ARGS="$ARGS -P 80 -p 8080"; HOST="${HOST}:80";;
  'https' ) ARGS="$ARGS -S 443";        HOST="https://${HOST}";;
  *       ) e2e_usage "Invalid test type";;
esac

# set rollout strategy
ARGS="$ARGS --rollout_strategy ${ESP_ROLLOUT_STRATEGY}"

trap cleanup EXIT

# Testing protocol
run kubectl create namespace "${NAMESPACE}" || error_exit "Namespace already exists"
run kubectl create -f ${YAML_FILE}          --namespace "${NAMESPACE}"
run $CLI deploy bookstore ${ESP_APP} $ARGS  --namespace "${NAMESPACE}"
run kubectl get services -o yaml            --namespace "${NAMESPACE}"
run kubectl get deployments -o yaml         --namespace "${NAMESPACE}"

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

  run retry -n 10 wait_for_service_config_rollouts_update "gke" "${HOST}:8090/endpoints_status" "$ESP_SERVICE_VERSION 100" \
    || error_exit 'Rollouts update was failed'
fi

run ${CLI} logs bookstore --namespace ${NAMESPACE} --project ${PROJECT_ID} --active=false \
  | tee ${LOG_DIR}/error.log

if [[ -n $REMOTE_LOG_DIR ]]; then
  upload_logs "${REMOTE_LOG_DIR}" "${LOG_DIR}"
  rm -rf "${LOG_DIR}"
fi

exit ${STATUS}
