#!/usr/bin/python
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

import json
import logging
import urllib3
try:
  from oauth2client.service_account import ServiceAccountCredentials
except ImportError:
  logging.warning("Could not import service_account.")

_GOOGLE_API_SCOPE = (
    "https://www.googleapis.com/auth/service.management.readonly")

# Metadata service path
_METADATA_PATH = "/computeMetadata/v1/instance"


class FetchError(Exception):
    """Error class for fetching and validation errors."""
    def __init__(self, code, message):
        self.code = code
        self.message = message
    def __str__(self):
        return self.message

def fetch_service_name(metadata):
    """Fetch service name from metadata URL."""
    url = metadata + _METADATA_PATH + "/attributes/endpoints-service-name"
    headers = {"Metadata-Flavor": "Google"}
    client = urllib3.PoolManager()
    try:
        response = client.request("GET", url, headers=headers)
    except:
        raise FetchError(1,
            "Failed to fetch service name from the metadata server: " + url)
    status_code = response.status

    if status_code != 200:
        message_template = "Fetching service name failed (url {}, status code {})"
        raise FetchError(1, message_template.format(url, status_code))

    name = response.data
    logging.info("Service name: " + name)
    return name


def fetch_service_config_id(metadata):
    """Fetch service config ID from metadata URL."""
    url = metadata + _METADATA_PATH + "/attributes/endpoints-service-config-id"
    headers = {"Metadata-Flavor": "Google"}
    client = urllib3.PoolManager()
    try:
        response = client.request("GET", url, headers=headers)
        if response.status != 200:
            message_template = "Fetching service config ID failed (url {}, status code {})"
            raise FetchError(1, message_template.format(url, response.status))
    except:
        url = metadata + _METADATA_PATH + "/attributes/endpoints-service-version"
        try:
            response = client.request("GET", url, headers=headers)
        except:
            raise FetchError(1,
                    "Failed to fetch service config ID from the metadata server: " + url)
        if response.status != 200:
            message_template = "Fetching service config ID failed (url {}, status code {})"
            raise FetchError(1, message_template.format(url, response.status))

    version = response.data
    logging.info("Service config ID:" + version)
    return version


def make_access_token(secret_token_json):
    """Construct an access token from service account token."""
    logging.info("Constructing an access token with scope " + _GOOGLE_API_SCOPE)
    credentials = ServiceAccountCredentials.from_json_keyfile_name(
        secret_token_json,
        scopes=[_GOOGLE_API_SCOPE])
    logging.info("Service account email: " + credentials.service_account_email)
    token = credentials.get_access_token().access_token
    return token


def fetch_access_token(metadata):
    """Fetch access token from metadata URL."""
    access_token_url = metadata + _METADATA_PATH + "/service-accounts/default/token"
    headers = {"Metadata-Flavor": "Google"}
    client = urllib3.PoolManager()
    try:
        response = client.request("GET", access_token_url, headers=headers)
    except:
        raise FetchError(1,
            "Failed to fetch access token from the metadata server: " + access_token_url)
    status_code = response.status

    if status_code != 200:
        message_template = "Fetching access token failed (url {}, status code {})"
        raise FetchError(1, message_template.format(access_token_url, status_code))

    token = json.loads(response.data)["access_token"]
    return token


def fetch_service_json(service_mgmt_url, access_token):
    """Fetch service config."""
    if access_token is None:
        headers = {}
    else:
        headers = {"Authorization": "Bearer {}".format(access_token)}

    client = urllib3.PoolManager()
    try:
        response = client.request("GET", service_mgmt_url, headers=headers)
    except:
        raise FetchError(1, "Failed to fetch service config")
    status_code = response.status

    if status_code != 200:
        message_template = "Fetching service config failed (status code {}, reason {}, url {})"
        raise FetchError(1, message_template.format(status_code, response.reason, service_mgmt_url))

    service_config = json.loads(response.data)
    return service_config


def validate_service_config(service_config, expected_service_name,
                            expected_service_version):
    """Validate service config."""
    service_name = service_config.get("name", None)

    if not service_name:
        raise FetchError(2, "No service name in the service config")

    if service_name != expected_service_name:
        message_template = "Unexpected service name in service config: {}"
        raise FetchError(2, message_template.format(service_name))

    service_version = service_config.get("id", None)

    if not service_version:
        raise FetchError(2, "No service config ID in the service config")

    if service_version != expected_service_version:
        message_template = "Unexpected service config ID in service config: {}"
        raise FetchError(2, message_template.format(service_version))

    # WARNING: sandbox migration workaround
    control = service_config.get("control", None)

    if not control:
        raise FetchError(2, "No control section in the service config")

    environment = control.get("environment", None)

    if not environment:
        raise FetchError(2, "Missing control environment")

    if environment == "endpoints-servicecontrol.sandbox.googleapis.com":
        logging.warning("Replacing sandbox control environment in the service config")
        service_config["control"]["environment"] = (
            "servicecontrol.googleapis.com")
