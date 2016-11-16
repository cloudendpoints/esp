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

function usage() {
  [[ -n "${1}" ]] && echo "${1}"
  echo "usage: ${BASH_SOURCE[0]} -g <path_to_gcloud> -v <version> -p <project id>"
  exit 1
}

GCLOUD=$(which gcloud) || GCLOUD=~/google-cloud-sdk/bin/gcloud
VERSION="error-$(date +%Y%m%d%H%M%S)"
PROJECT="esp-load-test"

while getopts g:v:p: arg; do
  case ${arg} in
    g) GCLOUD=${OPTARG};;
    v) VERSION=${OPTARG};;
    p) PROJECT=${OPTARG};;
    ?) usage "Invalid option: -${OPTARG}";;
  esac
done

echo "Deploying 'error' as ${VERSION} into ${PROJECT} project."
[[ -x "${GCLOUD}" ]] || usage "Cannot find gcloud, provide it via '-g' flag."

echo Y | ${GCLOUD} beta app deploy app.yaml \
  --project "${PROJECT}" \
  --version "${VERSION}" \
  --promote
