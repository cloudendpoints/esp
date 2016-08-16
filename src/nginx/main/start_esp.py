#!/usr/bin/python
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

import argparse
import json
import logging
import os
import sys
import urllib3

from oauth2client.service_account import ServiceAccountCredentials
from mako.template import Template

NGINX = "/usr/sbin/nginx"
NGINX_CONF_TEMPLATE = "/etc/nginx/nginx-auto.conf.template"
NGINX_CONF = "/etc/nginx/nginx-auto.conf"
SERVICE_JSON = "/etc/nginx/service.json"

_GOOGLE_API_SCOPE = (
    "https://www.googleapis.com/auth/service.management.readonly")
_METADATA_URL_PREFIX = (
    "http://metadata.google.internal/computeMetadata/v1/instance")
_SERVICE_MGMT_URL_TEMPLATE = (
    "https://servicemanagement.googleapis.com"
    "/v1/services/{}/config?configId={}")


logging.basicConfig()
logger = logging.getLogger('ESP')


def access_token(secret_token_json):
    credentials = ServiceAccountCredentials.from_json_keyfile_name(
        secret_token_json,
        scopes=[_GOOGLE_API_SCOPE])
    token = credentials.get_access_token().access_token
    return token


def fetch_access_token():
    access_token_url = _METADATA_URL_PREFIX + "/service-accounts/default/token"
    headers = {"Metadata-Flavor": "Google"}
    client = urllib3.PoolManager()
    response = client.request("GET", access_token_url, headers=headers)
    status_code = response.status

    if status_code != 200:
        message_template = "Fetching access token failed (status code {})"
        log_and_raise(-1, message_template.format(status_code))

    token = json.loads(response.data)["access_token"]
    return token


def fetch_service_json(url_template, service_name, service_version, access_token):
    service_mgmt_url = url_template.format(service_name, service_version)
    headers = {"Authorization": "Bearer {}".format(access_token)}
    client = urllib3.PoolManager()
    response = client.request("GET", service_mgmt_url, headers=headers)
    status_code = response.status

    if status_code != 200:
        message_template = "Fetching service config failed (status code {})"
        log_and_raise(-1, message_template.format(status_code))

    service_config = json.loads(response.data)
    validate_service_config(service_config, service_name, service_version)
    return service_config


def validate_service_config(service_config, expected_service_name,
                            expected_service_version):
    service_name = service_config.get("name", None)

    if not service_name:
        log_and_raise(-2, "No service name in the service config")

    if service_name != expected_service_name:
        message_template = "Unexpected service name in service config: {}"
        log_and_raise(-2, message_template.format(service_name))

    service_version = service_config.get("id", None)

    if not service_version:
        log_and_raise(-2, "No service version in the service config")

    if service_version != expected_service_version:
        message_template = "Unexpected service version in service config: {}"
        log_and_raise(-2, message_template.format(service_version))

    # WARNING: sandbox migration workaround
    environment = service_config["control"]["environment"]

    if not environment:
        log_and_raise(-3, "Missing control environment")

    if environment == "endpoints-servicecontrol.sandbox.googleapis.com":
        service_config["control"]["environment"] = (
            "servicecontrol.googleapis.com")


def log_and_raise(code, message):
    logger.error(message)
    sys.exit(code)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-k', help='Service account JSON key file (optional)')

    parser.add_argument('-s', help='Service name')
    parser.add_argument('-v', help='Service version')

    parser.add_argument('-c', help='NGINX configuration')
    parser.add_argument('-p', default='8080', help='ESP port')
    parser.add_argument('-N', default='8090', help='ESP status port')
    parser.add_argument('-a', default='localhost:8081', help='Backend address')
    parser.add_argument('-S', default='', help='ESP SSL port (optional)')

    parser.add_argument('--mgmt',
                        default=_SERVICE_MGMT_URL_TEMPLATE,
                        help='Service management URL template')
    parser.add_argument('--nginx',
                        default=NGINX,
                        help='Nginx binary')
    parser.add_argument('--json',
                        default=SERVICE_JSON,
                        help='Service JSON file location to place after fetch')


    args = parser.parse_args()

    # Fetch service configuration
    if args.s == None or args.v == None:
        log_and_raise(-4, "Please provide service name and version")
    if args.k != None:
        token = access_token(args.k)
    else:
        token = fetch_access_token()
    config = fetch_service_json(args.mgmt, args.s, args.v, token)

    # Save service json for ESP
    f = open(args.json, 'w+')
    json.dump(config, f, sort_keys=True, indent=2, separators=(',', ': '))
    f.close()

    # Create nginx.conf
    if args.c == None:
        template = Template(filename=NGINX_CONF_TEMPLATE)
        conf = template.render(
            port=args.p,
            status=args.N,
            backend=args.a,
            service_json=args.json,
            ssl=args.S,
            service_token=args.k)
        f = open(NGINX_CONF, 'w+')
        f.write(conf)
        f.close()
        nginx_conf = NGINX_CONF
    else:
        nginx_conf = args.c

    os.execv(args.nginx, ['nginx', '-p', '/usr', '-c', nginx_conf])
