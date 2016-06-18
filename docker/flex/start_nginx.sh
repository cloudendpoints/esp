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

CERT_DIR=/etc/ssl/localcerts
KEY_FILE=${CERT_DIR}/lb.key
CSR_FILE=${CERT_DIR}/lb.csr
CRT_FILE=${CERT_DIR}/lb.crt

mkdir -p ${CERT_DIR}
if [[ ! -f "${KEY_FILE}" ]]; then
  rm -f ${CSR_FILE} ${CRT_FILE}
  openssl genrsa -out ${KEY_FILE} 2048
  openssl req -new -key ${KEY_FILE} -out ${CSR_FILE} -subj "/"
  openssl x509 -req -in ${CSR_FILE} -signkey ${KEY_FILE} -out ${CRT_FILE}
fi

if [[ -n ${GAE_EXTRA_NGINX_CONFS} ]]; then
  for conf in ${GAE_EXTRA_NGINX_CONFS}; do
    if [[ -f /var/lib/nginx/optional/${conf} ]]; then
      cp /var/lib/nginx/optional/${conf}  /var/lib/nginx/extra
    fi
  done
fi

# Start crond so that log rotation works.
/usr/sbin/service cron restart

# use the override nginx.conf if there is one.
if [[ -f "${CONF_FILE}" ]]; then
  cp "${CONF_FILE}" /etc/nginx/nginx.conf
fi

/usr/sbin/nginx -p /usr -c /etc/nginx/nginx.conf
