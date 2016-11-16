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
# Debian package contents test.

# If not executed by "bazel test", do our best to succeed.
if [[ -z "${TEST_SRCDIR}" ]]; then
  TEST_SRCDIR="$(cd "${BASH_SOURCE[0]}.runfiles" && pwd)" \
    || { echo "Cannot determine TEST_SRCDIR directory."; exit 1; }
  TEST_WORKSPACE="${TEST_WORKSPACE:-__main__}"
  TEST_TMPDIR="$(mktemp -d)"
fi

ROOT="${TEST_SRCDIR}/${TEST_WORKSPACE}"

DEB_REL="src/nginx/main/endpoints-server-proxy.deb"
DEB_ABS="${ROOT}/${DEB_REL}"

# Utilities to operate on Debian packages on Mac and Linux.
# On Linux, tar cannot be used to open the .deb archive so we
# need to use ar.
function list_deb_Darwin() {
  local deb_file="${1}"
  tar tf "${deb_file}"
}

function list_deb_Linux() {
  local deb_file="${1}"
  ar tf "${deb_file}"
}

# Platform agnostic function which calls the platform specific one,
# based on retuls of $(uname), passing all arguments through unchanged.
function list_deb() {
  list_deb_$(uname) "$@"
}

function extract_deb_Darwin() {
  local deb_file="${1}"
  local directory="${2}"
  tar xfv "${deb_file}" -C "${directory}"
}

function extract_deb_Linux() {
  local deb_file="${1}"
  local directory="${2}"
  # Run in a sub-shell to preserve current directory.
  ( cd "${directory}" && ar xfv "${deb_file}" ) || return 1
  return 0
}

# Platform agnostic function which calls the platform specific one,
# based on retuls of $(uname), passing all arguments through unchanged.
function extract_deb() {
  extract_deb_$(uname) "$@"
}

function extract_tgz() {
  tar xzfv "${1}" -C "${2}"
}

function test_have_deb() {
  if ! [[ -f "${DEB_ABS}" ]]; then
    echo "Cannot find ${DEB_REL}"
    return 1
  fi
  return 0
}

function test_deb_contents() {
  diff "${ROOT}/src/nginx/main/testdata/deb-expected" - \
    < <(list_deb "${DEB_ABS}" | sort)
}

function test_control_file() {
  local dir="${TEST_TMPDIR}/control_file"
  mkdir -p "${dir}/deb" "${dir}/control" || return 1
  extract_deb "${DEB_ABS}" "${dir}/deb" || return 2
  [[ -f "${dir}/deb/control.tar.gz" ]] || return 3

  diff "${ROOT}/src/nginx/main/testdata/control-expected" - \
      < <(tar tzf "${dir}/deb/control.tar.gz" | sort) \
    || return 4
  extract_tgz "${dir}/deb/control.tar.gz" "${dir}/control" || return 5
  return 0
}

function test_data_file() {
  local dir="${TEST_TMPDIR}/data_file"
  mkdir -p "${dir}/deb" "${dir}/data" || return 1
  extract_deb "${DEB_ABS}" "${dir}/deb" || return 2
  [[ -f "${dir}/deb/data.tar.gz" ]] || return 3

  diff "${ROOT}/src/nginx/main/testdata/data-expected" - \
      < <(tar tzf "${dir}/deb/data.tar.gz" | sort) \
    || return 4

  extract_tgz "${dir}/deb/data.tar.gz" "${dir}/data" || return 5

  local copyright="${dir}/data/usr/share/doc/nginx-google/copyright"
  [[ -f "${copyright}" ]] || return 6

  diff "${copyright}" "${ROOT}/src/nginx/main/testdata/copyright-expected" \
    || return 7

  return 0
}

# Main body: iterate through tests and run them.
RESULT=0
while read d f test; do
  case $test in
    test_*)
      echo "+++++++++++++++++++++++++++++++++++++++++++++++++"
      echo " Running $test"
      echo "+++++++++++++++++++++++++++++++++++++++++++++++++"
      $test || { echo "${test} failed with $?"; RESULT=1; }
    ;;
  esac
done < <(declare -fF)

echo "Exiting with ${RESULT}"
exit ${RESULT}
