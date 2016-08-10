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
# This template script will be used when creating a vm with the
# --metadata-from-file startup-script (gcloud) flag.

# All TMPL_ variable will be replaced by actual values
# in the deploy_services.sh script.
VM_IMAGE='TMPL_VM_IMAGE'
REMOTE_BOOKSTORE_ARCHIVE='TMPL_BOOKSTORE_ARCHIVE'
TESTING_REMOTE_DEB='TMPL_TESTING_DEBIAN_PKG'
DIRECT_REPO='TMPL_DIRECT_REPO'
ESP_VERSION='TMPL_ESP_VERSION'

BOOKSTORE_ARCHIVE='bookstore.tar.gz'
# Retry operation
function retry() {
  local count=10
  local sleep_sec=10

  while : ; do
    "${@}" && { echo "Command succeeded."; return 0; }
    [[ "${count}" -le 0 ]] && { echo "Command '${@}' failed, aborting."; return 1; }
    sleep "${sleep_sec}"
    ((count--))
  done
}

# Echo and run a shell command, exit on failure
function run() {
  echo ""
  echo "[$(date)] $@"
  "${@}"
  local status=${?}
  if [[ "${status}" != "0" ]]; then
    echo "Command failed with exit status ${status}: ${@}" >&2
    exit 1
  fi
  return "${status}"
}

# Check installed version and compare it to expected version
function check_esp_version_debian_8() {
  local version=''
  version=$(dpkg -l | grep endpoints-runtime | awk '{print $3}')
  [[ "${version}" == "${ESP_VERSION}" ]] || \
    { echo 'ESP version &{version} is not expected ${ESP_VERSION}' ; exit 1; }
}

# Install Debian 8 packages
function install_pkg_debian8() {
  # Cleaning up apt-cache
  apt-get clean
  rm -rf /var/lib/apt/lists/*
  # apt-get update sometime fails because of remote servers.
  # if failures are related to esp, apt-get install will fail.
  apt-get update --fix-missing
  # Install dependencies.
  apt-get install -y npm endpoints-runtime supervisor && return 0
  return 1
}

# Install ESP and Bookstore for Debian 8
function install_debian_8() {
  if [[ -z "${DIRECT_REPO}" ]]; then
    export CLOUD_ENDPOINTS_REPO="google-cloud-endpoints-$(lsb_release -c -s)"
  else
    export CLOUD_ENDPOINTS_REPO="${DIRECT_REPO}"
  fi
  echo "${CLOUD_ENDPOINTS_REPO}"
  echo "deb http://packages.cloud.google.com/apt ${CLOUD_ENDPOINTS_REPO} main" \
    | tee /etc/apt/sources.list.d/google-cloud-endpoints.list
  curl --silent https://packages.cloud.google.com/apt/doc/apt-key.gpg \
    | apt-key add -
  run retry install_pkg_debian8
  if [[ -z "${DIRECT_REPO}" ]]; then
    # Installing the testing debian package with dpkg
    TESTING_LOCAL_DEB="enpoints-runtime.deb"
    run gsutil cp "${TESTING_REMOTE_DEB}" "${TESTING_LOCAL_DEB}"
    run dpkg -i "${TESTING_LOCAL_DEB}"
  fi
  check_esp_version_debian_8
}

case "${VM_IMAGE}" in
  "debian-8") install_debian_8;;
  *) echo "${VM_IMAGE} is not yet supported" && exit 1;;
esac

# Installing Bookstore
run mkdir -p /opt/bookstore
run pushd /opt/bookstore
run gsutil cp "${REMOTE_BOOKSTORE_ARCHIVE}" "${BOOKSTORE_ARCHIVE}"
run tar zxvf "${BOOKSTORE_ARCHIVE}"
run rm "${BOOKSTORE_ARCHIVE}"
run npm install
run popd

run cat << EOF > /etc/supervisor/conf.d/esp-bookstore.conf
[program:my-api]
command=nodejs /opt/bookstore/bookstore.js
environment=PORT=8081
autostart=true
stderr_logfile=/var/log/bookstore.err.log
stdout_logfile=/var/log/bookstore.out.log
EOF

# service supervisor restart fails 50% of the time.
run /etc/init.d/supervisor force-stop
run /etc/init.d/supervisor stop
run /etc/init.d/supervisor start
