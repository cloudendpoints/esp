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
# Wrapper script to start nginx with different options.

SERVER_ADDRESS='127.0.0.1:8081'
NGINX_CONF_TEMPLATE_PATH_BASE='/etc/nginx'
NGINX_CONF_TEMPLATE_PATH="${NGINX_CONF_TEMPLATE_PATH_BASE}/nginx.conf.template"
NGINX_CONF_PATH=''
SSL_CONF_TEMPLATE_PATH="${NGINX_CONF_TEMPLATE_PATH_BASE}/ssl.conf.template"
SSL_CONF_PATH=''
PORT=8080
SSL_PORT=''
NGINX_STATUS_PORT=8090
SERVICE_NAME=''
SERVICE_VERSION=''

usage () {
  cat << END_USAGE

Usage: $(command basename $0) [options]

Examples:
(1) Starts nginx at http port 80 and https port 443 that proxies the requests to
the application at 127.0.0.1:8000
$(command basename $0) -a 127.0.0.1:8000 -p 80 -S 443 -s SERVICE_NAME -v SERVICE_VERSION

(2) Starts nginx with a custom nginx config mounted at /app/nginx/
$(command basename $0) -n /app/nginx/nginx.conf -s SERVICE_NAME -v SERVICE_VERSION

Options:
    -h
        Shows this message.

    -s ENDPOINTS_SERVICE_NAME
        Required. The name of the Endpoints Service.
        e.g. my-service.my-project-id.appspot.com

    -v ENDPOINTS_SERVICE_VERSION
        Required. The version of the Endpoints Service which is assigned
        when deploying the service API specification.
        e.g. 2016-04-20R662

    -a HOST:PORT
        The application server address to which nginx will proxy the requests.
        Default is 127.0.0.1:8081.

    -n NGINX_CONF_PATH
        Specifies the path to a custom nginx.conf. If this option is specified,
        all other options will be ignored.

    -N PORT
        Port to expose /nginx_status. Default is 8090.

    -p PORT
        The nginx http port. Default is 8080.

    -S PORT
        Enables the nginx ssl port to serve https request.
        By default, the ssl port is disabled.

    -

END_USAGE
  exit 1
}

while getopts 'ha:n:N:p:S:s:v:' arg; do
  case ${arg} in
    h) usage;;
    a) SERVER_ADDRESS="${OPTARG}";;
    s) SERVICE_NAME="${OPTARG}";;
    v) SERVICE_VERSION="${OPTARG}";;
    n) NGINX_CONF_PATH="${OPTARG}";;
    N) NGINX_STATUS_PORT="${OPTARG}";;
    p) PORT="${OPTARG}";;
    S) SSL_PORT="${OPTARG}";;
    ?) usage;;
  esac
done

if [[ -z "${SERVICE_NAME}" ]]; then
  echo "-s SERVICE_NAME must be provided!"
  exit 3
fi

if [[ -z "${SERVICE_VERSION}" ]]; then
  echo "-v SERVICE_VERSION must be provided!"
  exit 4
fi

if [[ ! "${NGINX_CONF_PATH}" ]]; then
 NGINX_CONF_PATH="${NGINX_CONF_TEMPLATE_PATH_BASE}/nginx.conf"
 sed -e "s|\${SERVER_ADDRESS}|${SERVER_ADDRESS}|g" \
     -e "s|\${PORT}|${PORT}|g" \
     -e "s|\${NGINX_STATUS_PORT}|${NGINX_STATUS_PORT}|g" \
     ${NGINX_CONF_TEMPLATE_PATH} > "${NGINX_CONF_PATH}"

 if [[ "${SSL_PORT}" ]]; then
   SSL_CONF_PATH="${NGINX_CONF_TEMPLATE_PATH_BASE}/ssl.conf"
   sed -e "s|\${SSL_PORT}|${SSL_PORT}|g" \
       "${SSL_CONF_TEMPLATE_PATH}" > "${SSL_CONF_PATH}"
 else
   # remove ssl config
   sed -i "/ssl.conf/d" "${NGINX_CONF_PATH}"
 fi
fi

# Start crond so that log rotation works.
/usr/sbin/service cron restart

# Fetch service configuration from Service Management API
/usr/sbin/fetch_service_config.sh \
  -s "${SERVICE_NAME}" -v "${SERVICE_VERSION}" || exit $?

# Increase ephemeral port range
sudo sh -c 'echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse'
sudo sh -c 'echo 1 > /proc/sys/net/ipv4/tcp_tw_recycle'
sudo sh -c 'echo "1024 65000" > /proc/sys/net/ipv4/ip_local_port_range'

/usr/sbin/nginx -p /usr -c "${NGINX_CONF_PATH}"
