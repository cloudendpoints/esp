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

. ${ESP_SCRIPTS_PATH}/jenkins-utilities || { echo "Cannot load Jenkins Bash utilities" ; exit 1 ; }


function cleanup {
  if [[ "${SKIP_CLEANUP}" == 'false' ]]; then
    delete_service "${ESP_SERVICE}"
    gcloud compute instances delete "${INSTANCE_NAME}" -q
  fi
}

e2e_options "${@}"
[[ -n "${UNIQUE_ID}" ]] && INSTANCE_NAME="${UNIQUE_ID}"
[[ -n "${ENDPOINTS_RUNTIME_VERSION}" ]] || \
  e2e_usage "Must provide Enpoints runtime version via '-V' parameter."
[[ -n "${VM_IMAGE}" ]] || e2e_usage "Must provide VM image via '-v' parameter."
[[ -n "${INSTANCE_NAME}" ]] || e2e_usage "Must provide Instance name with '-i' parameter."

ARCHIVE_PATH="${ESP_ROOT}/bookstore.tar.gz"
BOOKSTORE_PATH="${SCRIPT_PATH}/../"
REMOTE_ARCHIVE_PATH="gs://${BUCKET}/${INSTANCE_NAME}.tar.gz"
VM_STARTUP_SCRIPT="${ESP_ROOT}/vm_startup_script.sh"
VM_STARTUP_SCRIPT_TMPL="${SCRIPT_PATH}/vm_startup_script_template.sh"

# Creating swagger template
run cp -f "${SWAGGER_TMPL}" swagger.json
sed_i "s|\${ENDPOINT_SERVICE}|${ESP_SERVICE}|g" swagger.json

# Creating and Deploying Service. This will export ESP_SERVICE_VERSION.
create_service "${ESP_SERVICE}" swagger.json

# Creating tar file
pushd "${BOOKSTORE_PATH}"
rm -rf "${ARCHIVE_PATH}"
run tar czvf "${ARCHIVE_PATH}" swagger.json package.json bookstore.js
popd

# Uploading Archive to Bucket
run retry -n 3 gsutil cp "${ARCHIVE_PATH}" "${REMOTE_ARCHIVE_PATH}"

# Creating
run cp -f "${VM_STARTUP_SCRIPT_TMPL}" "${VM_STARTUP_SCRIPT}"
sed_i "s|TMPL_VM_IMAGE|${VM_IMAGE}|g" "${VM_STARTUP_SCRIPT}"
sed_i "s|TMPL_BOOKSTORE_ARCHIVE|${REMOTE_ARCHIVE_PATH}|g" "${VM_STARTUP_SCRIPT}"
sed_i "s|TMPL_TESTING_DEBIAN_PKG|${DEBIAN_PKG}|g" "${VM_STARTUP_SCRIPT}"
sed_i "s|TMPL_ESP_VERSION|${ENDPOINTS_RUNTIME_VERSION}|g" "${VM_STARTUP_SCRIPT}"
sed_i "s|TMPL_DIRECT_REPO|${DIRECT_REPO}|g" "${VM_STARTUP_SCRIPT}"
run cat "${VM_STARTUP_SCRIPT}"

# Register cleanup for exit
trap cleanup EXIT

# Creating the VM
run retry -n 3 gcloud compute instances create "${INSTANCE_NAME}" \
  --machine-type "custom-2-3840" \
  --image-family "${VM_IMAGE}" \
  --image-project debian-cloud \
  --metadata endpoints-service-name="${ESP_SERVICE}",endpoints-service-version="${ESP_SERVICE_VERSION}" \
  --metadata-from-file startup-script="${VM_STARTUP_SCRIPT}"

run retry -n 3 get_host_ip "${INSTANCE_NAME}"
HOST="http://${HOST_INTERNAL_IP}:8080"

LOG_DIR="$(mktemp -d /tmp/log.XXXX)"
TEST_ID="gce-${VM_IMAGE}"
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
