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
SCRIPT_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ESP_ROOT="$(cd "${SCRIPT_PATH}/../" && pwd)"
ESP_SCRIPTS_PATH="$(cd "${ESP_ROOT}/script/" && pwd)"

. ${ESP_SCRIPTS_PATH}/jenkins-utilities || { echo "Cannot load Jenkins Bash utilities" ; exit 1 ; }

function cleanup {
  if [[ "${SKIP_CLEANUP}" == 'false' ]]; then
    run kubectl delete namespace "${NAMESPACE}"
  fi
}

e2e_options "${@}"

if [[ -z ${BOOKSTORE_IMAGE} ]]; then
  if [[ ${GRPC} != 'off' ]]; then
    BOOKSTORE_IMAGE="gcr.io/endpointsv2/grpc-${GRPC}-server:latest"
  else
    BOOKSTORE_IMAGE='gcr.io/endpoints-jenkins/bookstore:0.3'
  fi
fi

if [[ -z ${ESP_SERVICE} ]]; then
  if [[ ${GRPC} != 'off' ]]; then
    ESP_SERVICE="grpc-${GRPC}-dot-endpoints-jenkins.appspot.com"
  else
    ESP_SERVICE=${DEFAULT_ESP_SERVICE}
  fi
fi

CLI="$ESP_ROOT/bazel-bin/test/src/espcli"

if [[ ! -x ${CLI} ]]; then
  ${BAZEL} ${BAZEL_ARGS} build ${BAZEL_BUILD_ARGS} //test/src:espcli \
    || error_exit "Failed to build //test/src:espcli"
fi

ARGS="\
  -N 8090 \
  -e LoadBalancer \
  --access_log off \
  --service ${ESP_SERVICE} \
  --image ${ESP_IMAGE} \
  --sslCert ${ESP_ROOT}/test/src/utils/nginx.crt \
  --sslKey ${ESP_ROOT}/test/src/utils/nginx.key \
"

if [[ $GRPC != 'off' ]]; then
  ARGS="$ARGS -g"
fi

case "${COUPLING_OPTION}" in
  'loose' ) ;;
  'tight' ) ARGS="$ARGS --tight";;
  'custom') ARGS="$ARGS --tight -n ${ESP_ROOT}/test/bookstore/gke/nginx.conf";;
  *       ) e2e_usage "Invalid test option";;
esac

case "${TEST_TYPE}" in
  'http'  ) ARGS="$ARGS -p 80";         PORT_KEY="endpoints-http";;
  'http2' ) ARGS="$ARGS -P 80 -p 8080"; PORT_KEY="endpoints-http2";;
  'https' ) ARGS="$ARGS -S 443";        PORT_KEY="endpoints-ssl";;
  *       ) e2e_usage "Invalid test type";;
esac

# Testing protocol
NAMESPACE="${UNIQUE_ID}"
YAML_FILE=${ESP_ROOT}/test/src/deploy/bookstore.yaml
ESP_APP="esp"
run kubectl create namespace "${NAMESPACE}" || error_exit "Namespace already exists"
trap cleanup EXIT
run sed_i "s|image:.*|image: ${BOOKSTORE_IMAGE}|g" ${YAML_FILE}
run cat ${YAML_FILE}
run kubectl create -f ${YAML_FILE} --namespace ${NAMESPACE}
run $CLI deploy bookstore $ESP_APP $ARGS --namespace "${NAMESPACE}"
run kubectl get services -o yaml --namespace "${NAMESPACE}"
run kubectl get deployments -o yaml --namespace "${NAMESPACE}"
ENDPOINT=$(\
  $CLI endpoints bookstore --namespace ${NAMESPACE} |\
  python -c "import json,sys;obj=json.load(sys.stdin);print obj[\"$ESP_APP\"][\"$PORT_KEY\"]"
)

case "${TEST_TYPE}" in
  'http' ) HOST="http://${ENDPOINT}";;
  'http2') HOST="${ENDPOINT}";;
  'https') HOST="https://${ENDPOINT}";;
esac

echo "Service is available at: ${HOST}"

LOG_DIR="$(mktemp -d /tmp/log.XXXX)"
TEST_ID="gke-${COUPLING_OPTION}-${TEST_TYPE}"
[ "${GRPC}" != 'off' ] && TEST_ID="${TEST_ID}-${GRPC}"
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
run ${CLI} logs bookstore \
  --namespace ${NAMESPACE} \
  --project ${PROJECT_ID} \
  --active=false | tee ${LOG_DIR}/error.log

if [[ -n $REMOTE_LOG_DIR ]]; then
  upload_logs "${REMOTE_LOG_DIR}" "${LOG_DIR}"
  rm -rf "${LOG_DIR}"
fi

exit ${STATUS}
