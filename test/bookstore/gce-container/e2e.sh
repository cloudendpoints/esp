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
ESP_ROOT="$(cd "${SCRIPT_PATH}/../../../" && pwd)"
ESP_SCRIPTS_PATH="$(cd "${ESP_ROOT}/script/" && pwd)"

REMOTE_ESP_LOG_DIR='/var/log/esp'
. "${ESP_SCRIPTS_PATH}"/jenkins-utilities || { echo "Cannot load Jenkins Bash utilities" ; exit 1 ; }

function cleanup {
  if [[ "${SKIP_CLEANUP}" == 'false' ]]; then
    # Deactivating and deleting service
    delete_service "${ESP_SERVICE}"
    # Deleting vm
    gcloud compute instances delete "${INSTANCE_NAME}" -q
  fi
}

BOOKSTORE_PATH="${SCRIPT_PATH}/../"
SWAGGER_TMPL="${SCRIPT_PATH}/../swagger_template.json"
VM_STARTUP_SCRIPT="${SCRIPT_PATH}/vm_startup_script.sh"
YAML_TMPL="${SCRIPT_PATH}/esp_http_template.yaml"
YAML_FILE='esp_http.yaml'

e2e_options "${@}"
[[ -n "${UNIQUE_ID}" ]] && INSTANCE_NAME="${UNIQUE_ID}"
[[ -n "${BOOKSTORE_IMAGE}" ]] || e2e_usage "Must provide Bookstore docker image via '-b' parameter."
[[ -n "${ESP_IMAGE}" ]] || e2e_usage "Must provide esp docker image via '-e' parameter."
[[ -n "${INSTANCE_NAME}" ]] || e2e_usage "Must provide Instance name via 'i' parameter."

if [[ "${GRPC}" == 'true' ]]; then
  HOST="${INSTANCE_NAME}:8080"
  echo "grpc service name is: ${ESP_SERVICE}"
else
  HOST="http://${INSTANCE_NAME}:8080"
fi
# Creating swagger template
# TODO: refactor to reuse with raw GCE scripts
run cp -f ${SWAGGER_TMPL} swagger.json
sed_i "s|\${ENDPOINT_SERVICE}|${ESP_SERVICE}|g" swagger.json

# Creating and Deploying Service. This will export ESP_SERVICE_VERSION
create_service "${ESP_SERVICE}" swagger.json

# Register cleanup for exit
trap cleanup EXIT

# Creating Service template
run cp -f ${YAML_TMPL} ${YAML_FILE}
sed_i "s|\${BOOKSTORE_IMAGE}|${BOOKSTORE_IMAGE}|g" "${YAML_FILE}"
sed_i "s|\${ESP_IMAGE}|${ESP_IMAGE}|g" "${YAML_FILE}"
sed_i "s|\${SERVICE_NAME}|${ESP_SERVICE}|g" "${YAML_FILE}"
sed_i "s|\${SERVICE_VERSION}|${ESP_SERVICE_VERSION}|g" "${YAML_FILE}"
run cat "${YAML_FILE}"

# Creating the VM
run retry -n 3 gcloud compute instances create "${INSTANCE_NAME}" \
  --machine-type "custom-2-3840" \
  --image "${VM_IMAGE}" \
  --metadata-from-file google-container-manifest="${YAML_FILE}",startup-script="${VM_STARTUP_SCRIPT}"

LOG_DIR="$(mktemp -d /tmp/log.XXXX)"
TEST_ID="gce-${VM_IMAGE}"
[[ "${GRPC}" == 'true' ]] && TEST_ID="${TEST_ID}-grpc"

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
save_vm_logs "${INSTANCE_NAME}" "${LOG_DIR}"
upload_logs "${REMOTE_LOG_DIR}" "${LOG_DIR}"
rm -rf "${LOG_DIR}"
exit ${STATUS}
