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

#
# Start-up script for ESP.
# Configures nginx and fetches service configuration.
#
# Exit codes:
#     1 - failed to fetch,
#     2 - validation error,
#     3 - IO error,
#     4 - argument parsing error,
#     in addition to NGINX error codes.

import argparse
import collections
import fetch_service_config as fetch
import json
import logging
import os
import sys
import textwrap

from collections import Counter
from mako.template import Template

# Location of NGINX binary
NGINX = "/usr/sbin/nginx"

# Location of NGINX template
NGINX_CONF_TEMPLATE = "/etc/nginx/nginx-auto.conf.template"

# Location of generated config files
CONFIG_DIR = "/etc/nginx/endpoints"

# GRPC protocol prefix
GRPC_PREFIX = "grpc://"

# Metadata service
METADATA_ADDRESS = "http://169.254.169.254"

# Service management service
SERVICE_MGMT_URL_TEMPLATE = (
    "https://servicemanagement.googleapis.com"
    "/v1/services/{}/config?configId={}")

# DNS resolver
DNS_RESOLVER = "8.8.8.8"

# Default HTTP/1.x port
DEFAULT_PORT = 8080

# Default status port
DEFAULT_STATUS_PORT = 8090

# Default backend
DEFAULT_BACKEND = "127.0.0.1:8081"

# PID file (for nginx as a daemon)
PID_FILE = "/var/run/nginx.pid"

Port = collections.namedtuple('Port',
        ['port', 'proto'])
Location = collections.namedtuple('Location',
        ['path', 'backends', 'service_config', 'grpc'])
Ingress = collections.namedtuple('Ingress',
        ['ports', 'host', 'locations'])

def write_pid_file():
    try:
        f = open(PID_FILE, 'w+')
        f.write(str(os.getpid()))
        f.close()
    except IOError as err:
        logging.error("Failed to save PID file: " + PID_FILE)
        logging.error(err.strerror)
        sys.exit(3)

def write_template(ingress, nginx_conf, args):
    # Load template
    try:
        template = Template(filename=args.template)
    except IOError as err:
        logging.error("Failed to load NGINX config template. " + err.strerror)
        sys.exit(3)

    conf = template.render(
            ingress=ingress,
            pid_file=PID_FILE,
            status=args.status_port,
            service_account=args.service_account_key,
            metadata=args.metadata,
            resolver=args.dns,
            access_log=args.access_log,
            healthz=args.healthz)

    # Save nginx conf
    try:
        f = open(nginx_conf, 'w+')
        f.write(conf)
        f.close()
    except IOError as err:
        logging.error("Failed to save NGINX config." + err.strerror)
        sys.exit(3)


def ensure(config_dir):
    if not os.path.exists(config_dir):
        try:
            os.makedirs(config_dir)
        except OSError as exc:
            logging.error("Cannot create config directory.")
            sys.exit(3)


def assert_file_exists(fl):
    if not os.path.exists(fl):
        logging.error("Cannot find the specified file " + fl)
        sys.exit(3)


def start_nginx(nginx, nginx_conf):
    try:
        # Control is relinquished to nginx process after this line
        os.execv(nginx, ['nginx', '-p', '/usr', '-c', nginx_conf])
    except OSError as err:
        logging.error("Failed to launch NGINX: " + nginx)
        logging.error(err.strerror)
        sys.exit(3)


def fetch_service_config(args, service_config):
    try:
        # Fetch service config
        if args.service_config_url is None:
            if args.service is None:
                logging.info("Fetching the service name from the metadata service")
                args.service = fetch.fetch_service_name(args.metadata)

            if args.version is None:
                logging.info("Fetching the service version from the metadata service")
                args.version = fetch.fetch_service_version(args.metadata)

            service_mgmt_url = SERVICE_MGMT_URL_TEMPLATE.format(args.service,
                                                                args.version)
        else:
            service_mgmt_url = args.service_config_url

        # Get the access token
        if args.service_account_key is None:
            logging.info("Fetching an access token from the metadata service")
            token = fetch.fetch_access_token(args.metadata)
        else:
            token = fetch.make_access_token(args.service_account_key)

        logging.info("Fetching the service configuration from the service management service")
        config = fetch.fetch_service_json(service_mgmt_url, token)

        # Validate service config if we have service name version
        if args.service is not None and args.version is not None:
            fetch.validate_service_config(config, args.service, args.version)

        # Save service json for ESP
        try:
            f = open(service_config, 'w+')
            json.dump(config, f, sort_keys=True, indent=2,
                    separators=(',', ': '))
            f.close()
        except IOError as err:
            logging.error("Cannot save service config." + err.strerror)
            sys.exit(3)

    except fetch.FetchError as err:
        logging.error(err.message)
        sys.exit(err.code)


def make_ingress(service_config, args):
    ports = []

    # Set port by default
    if (args.http_port is None and
        args.http2_port is None and
        args.ssl_port is None):
        args.http_port = DEFAULT_PORT

    # Check for port collisions
    collisions = Counter([
            args.http_port, args.http2_port,
            args.ssl_port, args.status_port])
    collisions.pop(None, 0)
    if len(collisions) > 0:
        shared_port, count = collisions.most_common(1)[0]
        if count > 1:
            logging.error("Port " + str(shared_port) + " is used more than once.")
            sys.exit(2)

    if not args.http_port is None:
        ports.append(Port(args.http_port, "http"))
    if not args.http2_port is None:
        ports.append(Port(args.http2_port, "http2"))
    if not args.ssl_port is None:
        ports.append(Port(args.ssl_port, "ssl"))

    if args.backend.startswith(GRPC_PREFIX):
        grpc = True
        backends = [args.backend[len(GRPC_PREFIX):]]
    else:
        grpc = False
        backend = args.backend
        if backend.startswith("http://"):
            backend = backend[len("http://"):]
        elif backend.startswith("https://"):
            logging.error("https:// protocol for the backend server is not supported.")
            sys.exit(2)
        backends = [backend]

    locations = [Location(
            path='/',
            backends=backends,
            service_config=service_config,
            grpc=grpc)]

    ingress = Ingress(
            ports=ports,
            host='""',
            locations=locations)

    return ingress

class ArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        self.print_help(sys.stderr)
        self.exit(4, '%s: error: %s\n' % (self.prog, message))

def make_argparser():
    parser = ArgumentParser(
        description = textwrap.dedent('''
        ESP start-up script. This script fetches the service configuration from
        the service management service and configures ESP to expose the specified
        ports and proxy requests to the specified backend.

        The service name and version are optional. If not supplied, the script
        fetches the service name and version from the metadata service.

        The service account key file is used to generate an access token for the
        service management service. If the service account key file is not provided,
        the script fetches an access token from the metadata service.

        If a custom nginx config file is provided (-n flag), the script launches ESP
        with the provided config file. Otherwise, the script uses the exposed ports
        (-p, -P, -S, -N) and the backend (-a) to generate an nginx config file.
        '''))

    parser.add_argument('-k', '--service_account_key', help=''' Use the service
    account key JSON file to access the service control and the service
    management.  If the option is omitted, ESP contacts the metadata service to
    fetch an access token.  ''')

    parser.add_argument('-s', '--service', help=''' Set the name of the
    Endpoints service.  If omitted and -c not specified, ESP contacts the
    metadata service to fetch the service name.  ''')

    parser.add_argument('-v', '--version', help=''' Set the config version of
    the Endpoints service.  If omitted and -c not specified, ESP contacts the
    metadata service to fetch the service version.  ''')

    parser.add_argument('-n', '--nginx_config', help=''' Use a custom nginx
    config file instead of the config template {template}. If you specify this
    option, then all the port options are ignored.
    '''.format(template=NGINX_CONF_TEMPLATE))

    parser.add_argument('-p', '--http_port', default=None, type=int, help='''
    Expose a port to accept HTTP/1.x connections.  By default, if you do not
    specify any of the port options (-p, -P, and -S), then port {port} is
    exposed as HTTP/1.x port. However, if you specify any of the port options,
    then only the ports you specified are exposed, which may or may not include
    HTTP/1.x port.  '''.format(port=DEFAULT_PORT))

    parser.add_argument('-P', '--http2_port', default=None, type=int, help='''
    Expose a port to accept HTTP/2 connections.  Note that this cannot be the
    same port as HTTP/1.x port.  ''')

    parser.add_argument('-S', '--ssl_port', default=None, type=int, help='''
    Expose a port for HTTPS requests.  Accepts both HTTP/1.x and HTTP/2
    secure connections.  ''')

    parser.add_argument('-N', '--status_port', default=DEFAULT_STATUS_PORT,
    type=int, help=''' Change the ESP status port. Status information is
    available at /endpoints_status location over HTTP/1.x. Default value:
    {port}.'''.format(port=DEFAULT_STATUS_PORT))

    parser.add_argument('-a', '--backend', default=DEFAULT_BACKEND, help='''
    Change the application server address to which ESP proxies the requests.  For
    GRPC backends, please use grpc:// prefix, e.g. grpc://127.0.0.1:8081.
    Default value: {backend}.'''.format(backend=DEFAULT_BACKEND))

    parser.add_argument('-c', '--service_config_url', default=None, help='''
    Use the specified URL to fetch the service configuration instead of using
    the default URL template
    {template}.'''.format(template=SERVICE_MGMT_URL_TEMPLATE))

    parser.add_argument('-z', '--healthz', default=None, help='''Define a
    health checking endpoint on the same ports as the application backend. For
    example, "-z healthz" makes ESP return code 200 for location "/healthz",
    instead of forwarding the request to the backend.  Default: not used.''')

    # Specify a custom service.json path.
    # If this is specified, service json will not be fetched.
    parser.add_argument('--service_json_path',
        default=None,
        help=argparse.SUPPRESS)

    # Customize metadata service url prefix.
    parser.add_argument('-m', '--metadata',
        default=METADATA_ADDRESS,
        help=argparse.SUPPRESS)

    # Fetched service config and generated nginx config are placed
    # into config_dir as service.json and nginx.conf files
    parser.add_argument('--config_dir',
        default=CONFIG_DIR,
        help=argparse.SUPPRESS)

    # nginx.conf template
    parser.add_argument('--template',
        default=NGINX_CONF_TEMPLATE,
        help=argparse.SUPPRESS)

    # nginx binary location
    parser.add_argument('--nginx',
        default=NGINX,
        help=argparse.SUPPRESS)

    # Address of the DNS resolver used by nginx http.cc
    parser.add_argument('--dns',
        default=DNS_RESOLVER,
        help=argparse.SUPPRESS)

    # Access log destination. Use special value 'off' to disable.
    parser.add_argument('--access_log',
        default='/dev/stdout',
        help=argparse.SUPPRESS)

    return parser


if __name__ == '__main__':
    parser = make_argparser()
    args = parser.parse_args()
    logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.INFO)

    # Write pid file for the supervising process
    write_pid_file()

    # Get service config
    service_config = args.config_dir + "/service.json"

    if args.service_json_path:
        service_config = args.service_json_path
        assert_file_exists(service_config)
    else:
        # Fetch service config and place it in the standard location
        ensure(args.config_dir)
        fetch_service_config(args, service_config)

    # Start NGINX
    if args.nginx_config != None:
        start_nginx(args.nginx, args.nginx_config)
    else:
        ingress = make_ingress(service_config, args)
        nginx_conf = args.config_dir + "/nginx.conf"
        ensure(args.config_dir)
        write_template(ingress, nginx_conf, args)
        start_nginx(args.nginx, nginx_conf)

