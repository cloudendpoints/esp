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
# This script assumes following file locations:
# /usr/sbin/nginx for nginx binary
# /etc/nginx/nginx.conf for nginx config
#
# Usage:  /usr/sbin/debug_nginx.sh [ start | stop ]
#
#  start:  use debug Nginx binary and turn on debug trace
#  stop:  use release Nginx binary and turn off debug trace.
#

function start_debug() {
  [[ -e /etc/nginx/nginx.conf.original ]] || cp /etc/nginx/nginx.conf /etc/nginx/nginx.conf.original
  [[ -e /usr/sbin/nginx.original ]] || cp /usr/sbin/nginx /usr/sbin/nginx.original

  echo "worker_rlimit_core 512m;" >> /etc/nginx/nginx.conf-debug
  echo "working_directory /tmp;" >> /etc/nginx/nginx.conf-debug
  cat /etc/nginx/nginx.conf.original >> /etc/nginx/nginx.conf-debug

  cp /etc/nginx/nginx.conf-debug /etc/nginx/nginx.conf
  cp /usr/sbin/nginx-debug /usr/sbin/nginx

  ulimit -c unlimited

  nginx -s reload
}

function stop_debug() {
  [[ -e /etc/nginx/nginx.conf.original ]] && cp /etc/nginx/nginx.conf.original /etc/nginx/nginx.conf
  [[ -e /usr/sbin/nginx.original ]] && cp /usr/sbin/nginx.original /usr/sbin/nginx

  nginx -s reload
}

case $1 in
  start)
    start_debug
    ;;
  stop)
    stop_debug
    ;;
  *)
    echo "Usage: $0 {start|stop}"
    exit 2
   ;;
esac


