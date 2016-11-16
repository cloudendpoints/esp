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
# Helper script for fetching the Endpoints Service Configuration from
# Google Service Management API.
#

# Constants
MAX_RETRIES=5
MAX_RETRY_DELAY=60 # 1 minute
CURL_FLAGS=("--fail --show-error --silent --retry ${MAX_RETRIES} \
             --retry-max-time ${MAX_RETRY_DELAY}")
METADATA_URL_PREFIX='http://metadata.google.internal/computeMetadata/v1/instance'
SERVICE_MGMT_URL_PREFIX='https://servicemanagement.googleapis.com/v1/services'
OUTPUT_DIR='/etc/nginx/endpoints'
OUTPUT_FILE="${OUTPUT_DIR}/service.json"
SERVICE_NAME=''
SERVICE_VERSION=''

usage () {
  cat << END_USAGE
Usage: $(command basename $0) [options]
Examples:
Options:
    -h
        Shows this message.
    -s ENDPOINTS_SERVICE_NAME
        The Endpoints service name. If this flag is not provided,
        the script attempts to read the GCE metadata value
        "endpoints-service-name" instead.
    -v ENDPOINTS_SERVICE_CONFIG_ID
        The Endpoints service config ID. If this flag is not provided,
        the script attempts to read the GCE metadata value
        "endpoints-service-config-id" instead.

END_USAGE
  exit 1
}
while getopts 'v:s:h' arg; do
  case "${arg}" in
    h) usage;;
    v) SERVICE_VERSION="${OPTARG}";;
    s) SERVICE_NAME="${OPTARG}";;
    ?) usage;;
  esac
done

# Exit error codes:
# 0: success
# 1: failure to read data from metadata server
# 2: failure to extract data from input
# 3: unexpected data extracted from input
# 4: failure to call Service Management API

if [[ -z "${SERVICE_NAME}" ]] ; then
  # Get the service name metadata service
  SERVICE_NAME=$(/usr/bin/curl ${CURL_FLAGS} -H "Metadata-Flavor: Google" \
                 "${METADATA_URL_PREFIX}/attributes/endpoints-service-name")
  if [[ $? -ne 0 ]]; then
    echo "Failed to read metadata with key endpoints-service-name from metadata server!!!"
    exit 1
  fi
fi
echo "Endpoints service name: ${SERVICE_NAME}"
if [[ -z "${SERVICE_VERSION}" ]] ; then
  # Get the service version from metadata service
  SERVICE_VERSION=$(/usr/bin/curl ${CURL_FLAGS} -H "Metadata-Flavor: Google" \
                    "${METADATA_URL_PREFIX}/attributes/endpoints-service-config-id")
  if [[ $? -ne 0 ]]; then
    SERVICE_VERSION=$(/usr/bin/curl ${CURL_FLAGS} -H "Metadata-Flavor: Google" \
                    "${METADATA_URL_PREFIX}/attributes/endpoints-service-version")
    if [[ $? -ne 0 ]]; then
      echo "Failed to read metadata with key endpoints-service-config-id from metadata server!!!"
      exit 1
    fi
  fi
fi
echo "Endpoints service config ID: ${SERVICE_VERSION}"

function validateOutputFileContents() {
  local retrieved_svc_name=$(
    python -c "import json; print json.load(open(\"${OUTPUT_FILE}\"))['name']")
  if [[ $? -ne 0 ]]; then
    echo "Failed to extract Endpoints Service Name from ${OUTPUT_FILE}!!!"
    return 2
  fi
  local retrieved_svc_version=$(
    python -c "import json; print json.load(open(\"${OUTPUT_FILE}\"))['id']")
  if [[ $? -ne 0 ]]; then
    echo "Failed to extract Endpoints Service Version from ${OUTPUT_FILE}!!!"
    return 2
  fi
  if [[ "${retrieved_svc_name}" != "${SERVICE_NAME}" ]]; then
    echo "Unexpected service name ${retrieved_svc_name} in ${OUTPUT_FILE} !!!" \
         "Expected ${SERVICE_NAME}"
    return 3
  fi
  if [[ "${retrieved_svc_version}" != "${SERVICE_VERSION}" ]]; then
    echo "Unexpected service version ${retrieved_svc_version} in ${OUTPUT_FILE} !!!" \
         "Expected ${SERVICE_VERSION}"
    return 3
  fi
  echo "Success: ESP configuration for service ${SERVICE_NAME}" \
       "and version ${SERVICE_VERSION} is in ${OUTPUT_FILE}."
  return 0
}

# If ${OUTPUT_FILE} already exists, make sure it has the parameters we expect.
# Otherwise, download the service configuration and over-write ${OUTPUT_FILE}
if [[ -f "${OUTPUT_FILE}" ]] ; then
  echo "Service Configuration file ${OUTPUT_FILE} already exists!"
  validateOutputFileContents
  if [[ $? -eq 0 ]]; then
    exit 0
  fi
fi

mkdir -p ${OUTPUT_DIR}

# Fetch an authentication token to use for calling Service Management API
auth_token_json=$(/usr/bin/curl ${CURL_FLAGS} -H "Metadata-Flavor: Google" \
                  "${METADATA_URL_PREFIX}/service-accounts/default/token")
if [[ $? -ne 0 ]]; then
  echo "Failed to obtain Authentication Token from metadata server!!!"
  exit 1
fi
auth_token=$(/bin/echo "${auth_token_json}" | \
             python -c 'import sys, json; print json.load(sys.stdin)["access_token"]')
if [[ $? -ne 0 ]]; then
  echo "Failed to extract Authentication Token from metadata server response!!!"
  exit 2
fi

# TODO: consider refreshing the access token in the unlikely event that
# it expires while retrying the next call.
#
# Fetch the Service Configuration
service_mgmt_url="${SERVICE_MGMT_URL_PREFIX}/${SERVICE_NAME}/config?configId=${SERVICE_VERSION}"
echo "Downloading Endpoints Service Configuration to ${OUTPUT_FILE}"
config=$(/usr/bin/curl ${CURL_FLAGS} -H "Metadata-Flavor: Google" \
         -H "Authorization: Bearer ${auth_token}" \
         --output "${OUTPUT_FILE}" \
         "${service_mgmt_url}")
if [[ $? -ne 0 ]]; then
  echo "Failed to obtain Endpoints Service Configuration from Service Management API"
  exit 4
fi

validateOutputFileContents
return_code=$?
if [[ "${return_code}" -eq 0 ]]; then
  exit 0
else
  echo "Failed. Exiting with status code ${return_code}"
  exit "${return_code}"
fi
