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

function service_endpoint() {
  local namespace="${1}"
  local service="${2}"
  local endpoint="$(kubectl describe service "${service}" --namespace "${namespace}" \
    | grep "LoadBalancer Ingress:" | awk '{print $3}')"
  if [[ "${endpoint}" =~ [0-9\.\:] ]]; then
    echo "${endpoint}"
    return 0
  fi
  return 1
}

function service_ready() {
  local namespace="${1}"
  local service="${2}"
  local endpoint=''
  retry service_endpoint "${namespace}" "${service}"
  [[ "${?}" -eq 0 ]] || error_exit "Service ${service} is not ready"
}

function create_kb8_namespace() {
  local namespace="${1}"
  local retries=5
  kubectl get namespace "${namespace}"
  local namespace_exists="${?}"
  if [[ "${namespace_exists}" -eq 0 ]]; then
    echo "Namespace already exist, removing exiting one and associated services"
    kubectl delete namespace "${namespace}"
    until [[ (( ${retries} -le 0 || "${namespace_exists}" -ne 0 )) ]]; do
      sleep 5
      kubectl get namespace "${namespace}"
      namespace_exists="${?}"
    done
    [[ "${namespace_exists}" -ne 0 ]] || errot_exit "Namespace ${namespace} could not be deleted"
  fi
  run kubectl create namespace "${namespace}"
}

function cleanup {
  if [[ "${SKIP_CLEANUP}" == 'false' ]]; then
    # Deleting and deactivating service
    delete_service "${ESP_SERVICE}"
    # Delete namespace
    kubectl delete namespace "${NAMESPACE}"
  fi
}

# Extract logs from pods in a given namespace and upload it to a
# given directory.
function save_logs() {
  local namespace="${1}"
  local log_dir="${2}"
  local pods=($(kubectl get pods --namespace="${namespace}" -o=name))
  local tar_files=()

  for pod_name in ${pods[@]}; do
    local pod="${pod_name##*/}"
    local tar_file="${log_dir}/${pod}.tar.gz"
    kubectl exec "${pod}" \
      --namespace="${namespace}" \
      tar czvf - /var/log/ > "${tar_file}"
    [[ ${?} -eq 0 ]] || echo "Failed to get logs from pod ${pod}"
  done
}

BOOKSTORE_YAML="${ESP_ROOT}/bookstore.yaml"
NGINX_CONF_TMPL="${ESP_ROOT}/docker/generic/nginx.conf.template"
NGINX_CONF="${ESP_ROOT}/nginx.conf"
SERVER_ADDRESS=''
SWAGGER_TMPL="${SCRIPT_PATH}/../swagger_template.json"
SWAGGER_JSON="${ESP_ROOT}/swagger.json"

e2e_options "${@}"
[[ -n "${UNIQUE_ID}" ]] && NAMESPACE="${UNIQUE_ID}"
[[ -n "${BOOKSTORE_IMAGE}" ]] || e2e_usage "Must provide Bookstore docker image via '-b' parameter."
[[ -n "${ESP_IMAGE}" ]] || e2e_usage "Must provide esp docker image via '-e' parameter."
[[ -n "${COUPLING_OPTION}" ]] || e2e_usage "Must provide coulping option with '-c' parameter."
[[ -n "${NAMESPACE}" ]] || e2e_usage "Must provide namespace option with '-i' parameter."

case "${COUPLING_OPTION}" in
  'loose')
    YAML_DIR="${SCRIPT_PATH}/loose_coupling"
    BOOKSTORE_YAML_TMPL="${YAML_DIR}/bookstore_template.yaml"
    KB_SERVICE='esp'
    case "${TEST_TYPE}" in
      'http'|'grpc')
        YAML_TMPL="${YAML_DIR}/esp_http_template.yaml"
        YAML_FILE='esp_http.yaml';;
      'https')
        YAML_TMPL="${YAML_DIR}/esp_template.yaml"
        YAML_FILE='esp.yaml';;
      'custom')
        YAML_TMPL="${YAML_DIR}/esp_custom_config_template.yaml"
        YAML_FILE='esp_custom_config.yaml'
        SERVER_ADDRESS='bookstore:8081';;
      *) e2e_usage "Invalid option -t"
    esac;;
  'tight')
    YAML_DIR="${SCRIPT_PATH}/tight_coupling"
    KB_SERVICE='esp-bookstore'
    case "${TEST_TYPE}" in
      'http'|'grpc')
        YAML_TMPL="${YAML_DIR}/esp_bookstore_http_template.yaml"
        YAML_FILE='esp_bookstore_http.yaml';;
      'https')
        YAML_TMPL="${YAML_DIR}/esp_bookstore_template.yaml"
        YAML_FILE='esp_bookstore.yaml';;
      'custom')
        YAML_TMPL="${YAML_DIR}/esp_bookstore_custom_config_template.yaml"
        YAML_FILE='esp_bookstore_custom_config.yaml'
        SERVER_ADDRESS='127.0.0.1:8081';;
      *) e2e_usage "Invalid option -t"
    esac;;
  *) e2e_usage "Invalid option -c"
esac

# Creating kubernetes namespace
create_kb8_namespace "${NAMESPACE}"
# Registering cleanup at exit
trap cleanup EXIT

# Creating swagger template
run cp -f "${SWAGGER_TMPL}" "${SWAGGER_JSON}"
sed_i "s|\${ENDPOINT_SERVICE}|${ESP_SERVICE}|g" "${SWAGGER_JSON}"

# Creating and Deploying Service. This will export ESP_SERVICE_VERSION.
create_service "${ESP_SERVICE}" "${SWAGGER_JSON}"

# Creating Service template
run cp -f "${YAML_TMPL}" "${YAML_FILE}"
if [ -z "${BOOKSTORE_YAML_TMPL}" ]; then
  sed_i "s|\${BOOKSTORE_IMAGE}|${BOOKSTORE_IMAGE}|g" "${YAML_FILE}"
else
  run cp -f "${BOOKSTORE_YAML_TMPL}" "${BOOKSTORE_YAML}"
  sed_i "s|\${BOOKSTORE_IMAGE}|${BOOKSTORE_IMAGE}|g" "${BOOKSTORE_YAML}"
fi
sed_i "s|\${ESP_IMAGE}|${ESP_IMAGE}|g" "${YAML_FILE}"
sed_i "s|\${SERVICE_NAME}|${ESP_SERVICE}|g" ${YAML_FILE}
sed_i "s|\${SERVICE_VERSION}|${ESP_SERVICE_VERSION}|g" ${YAML_FILE}
run cat "${YAML_FILE}"

if [[ "${TEST_TYPE}" == "https" ]]; then
  run kubectl create secret generic nginx-ssl \
    --from-file="${SCRIPT_PATH}/nginx.key" \
    --from-file="${SCRIPT_PATH}/nginx.crt" \
    --namespace="${NAMESPACE}"
fi
if [[ "${TEST_TYPE}" == "custom" ]]; then
  run cp "${NGINX_CONF_TMPL}" "${NGINX_CONF}"
  sed_i "s|\${SERVER_ADDRESS}|${SERVER_ADDRESS}|g" "${NGINX_CONF}"
  sed_i "s|\${PORT}|8080|g" "${NGINX_CONF}"
  sed_i "s|\${NGINX_STATUS_PORT}|8090|g" "${NGINX_CONF}"
  sed_i "/ssl.conf/d" "${NGINX_CONF}"
  run cat "${NGINX_CONF}"
  run kubectl create configmap nginx-config \
    --from-file="${NGINX_CONF}" \
    --namespace="${NAMESPACE}"
fi

# Creating services in kubernetes
if [[ -n "${BOOKSTORE_YAML_TMPL}" ]]; then
  run kubectl create -f "${BOOKSTORE_YAML}" --namespace "${NAMESPACE}"
fi
run kubectl create -f "${YAML_FILE}" --namespace "${NAMESPACE}"
# Checking that Service is ready
run service_ready "${NAMESPACE}" "${KB_SERVICE}"
# Printing Endpoint
ENDPOINT="$(service_endpoint "${NAMESPACE}" "${KB_SERVICE}")"
echo "Service is available at: ${ENDPOINT}"

case "${TEST_TYPE}" in
  'https') HOST="https://${ENDPOINT}:443";;
  'grpc') HOST="${ENDPOINT}:8080";;
  *) HOST="http://${ENDPOINT}:8080";;
esac


LOG_DIR="$(mktemp -d /tmp/log.XXXX)"
TEST_ID="gke-${COUPLING_OPTION}-coupling-${TEST_TYPE}"
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
save_logs "${NAMESPACE}" "${LOG_DIR}"
upload_logs "${REMOTE_LOG_DIR}" "${LOG_DIR}"
rm -rf "${LOG_DIR}"
exit ${STATUS}
