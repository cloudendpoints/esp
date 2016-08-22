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
import os
import sys
import textwrap

from mako.template import Template

# Location of NGINX binary
NGINX = "/usr/sbin/nginx"

# Location of NGINX template
NGINX_CONF_TEMPLATE = "/etc/nginx/nginx-auto.conf.template"

# Location of generated config files
CONFIG_DIR = "/etc/nginx/endpoints"

# GRPC protocol prefix
GRPC_PREFIX = "grpc://"

Port = collections.namedtuple('Port',
        ['port', 'proto'])
Location = collections.namedtuple('Location',
        ['path', 'backends', 'service_config', 'grpc'])
Ingress = collections.namedtuple('Ingress',
        ['ports', 'host', 'locations'])

def write_template(template, status, service_account, ingress, nginx_conf):
    # Load template
    try:
        template = Template(filename=template)
    except IOError as err:
        print "ERROR: Failed to load NGINX config template.", err.strerror
        sys.exit(3)

    conf = template.render(
            status=status,
            service_account=service_account,
            ingress=ingress)

    # Save nginx conf
    try:
        f = open(nginx_conf, 'w+')
        f.write(conf)
        f.close()
    except IOError as err:
        print "ERROR: Failed to save NGINX config.", err.strerror
        sys.exit(3)


def ensure(config_dir):
    if not os.path.exists(config_dir):
        try:
            os.makedirs(config_dir)
        except OSError as exc:
            print "ERROR: Cannot create config directory."
            sys.exit(3)


def start_nginx(nginx, nginx_conf):
    try:
        # Control is relinquished to nginx process after this line
        os.execv(nginx, ['nginx', '-p', '/usr', '-c', nginx_conf])
    except OSError as err:
        print "ERROR: Failed to launch NGINX", err.strerror
        sys.exit(3)


def fetch_service_config(args, service_config):
    try:
        # Fetch service config
        if args.service is None:
            args.service = fetch.fetch_service_name(args.metadata_url_prefix)

        if args.version is None:
            args.version = fetch.fetch_service_version(args.metadata_url_prefix)

        if args.service_account_key is None:
            token = fetch.fetch_access_token(args.metadata_url_prefix)
        else:
            token = fetch.make_access_token(args.service_account_key)

        config = fetch.fetch_service_json(
                args.service_management,
                args.service,
                args.version,
                token)

        # Save service json for ESP
        try:
            f = open(service_config, 'w+')
            json.dump(config, f, sort_keys=True, indent=2,
                    separators=(',', ': '))
            f.close()
        except IOError as err:
            print "ERROR: Cannot save service config.", err.strerror
            sys.exit(3)

    except fetch.FetchError as err:
        print "ERROR:", err.message
        sys.exit(err.code)


def make_ingress(service_config, args):
    ports = []

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
        backends = [args.backend]

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
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
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

    parser.add_argument('-k', '--service_account_key', help='''
        Set the service account key JSON file.
        Used to access the service control and the service management.
        If omitted, ESP contacts the metadata service to fetch an access token.
    ''')

    parser.add_argument('-s', '--service', help='''
        Set the name of the Endpoints service.
        If omitted, ESP contacts the metadata service to fetch the service name.
    ''')

    parser.add_argument('-v', '--version', help='''
        Set the config version of the Endpoints service.
        If omitted, ESP contacts the metadata service to fetch the service version.
    ''')

    parser.add_argument('-n', '--nginx_config', help='''
        Set a custom nginx config file.
    ''')

    parser.add_argument('-p', '--http_port', default=None, type=int, help='''
        Expose a port to accept HTTP/1.x connections.
    ''')

    parser.add_argument('-P', '--http2_port', default=None, type=int, help='''
        Expose a port to accept HTTP/2 connections.
        Note that this cannot be the same port as HTTP/1.x port.
    ''')

    parser.add_argument('-S', '--ssl_port', default=None, type=int, help='''
        Expose a port for HTTPS requests.
        Accepts both HTTP/1.x and HTTP/2 connections.
    ''')

    parser.add_argument('-N', '--status_port', default=8090, type=int, help='''
        Set the ESP status port. Status information is available at /endpoints_status
        location over HTTP/1.x.
    ''')

    parser.add_argument('-a', '--backend', default='localhost:8081', help='''
        Set the application server address to which ESP proxies the requests.
        For GRPC backends, please use grpc:// prefix, e.g. grpc://localhost:8081.
    ''')

    parser.add_argument('-m', '--service_management',
        default=fetch._SERVICE_MGMT_URL_TEMPLATE, help='''
        Specify the service management service URL template. The template string takes
        the service name and the service version as parameters.
    ''')

    # These two flags enable or disable all fetching by the script
    # (metadata server and service management service)
    # This is useful with custom nginx config and custom service json
    parser.add_argument('--fetch', dest='fetch', action='store_true',
        help=argparse.SUPPRESS)

    parser.add_argument('--no-fetch', dest='fetch', action='store_false',
        help=argparse.SUPPRESS)

    parser.set_defaults(fetch=True)

    # Customize metadata service url prefix.
    parser.add_argument('-u', '--metadata_url_prefix',
        default=fetch._METADATA_URL_PREFIX,
        help=argparse.SUPPRESS)

    # Fetched service config and generated nginx config are placed
    # into config_dir as service.json and nginx.conf files
    parser.add_argument('--config_dir', default=CONFIG_DIR,
        help=argparse.SUPPRESS)

    # nginx.conf template
    parser.add_argument('--template', default=NGINX_CONF_TEMPLATE,
        help=argparse.SUPPRESS)

    # nginx binary location
    parser.add_argument('--nginx', default=NGINX,
        help=argparse.SUPPRESS)

    return parser


if __name__ == '__main__':
    parser = make_argparser()
    args = parser.parse_args()

    service_config = args.config_dir + "/service.json"

    # Fetch service config
    if args.fetch:
        ensure(args.config_dir)
        fetch_service_config(args, service_config)

    # Start NGINX
    if args.nginx_config != None:
        start_nginx(args.nginx, args.nginx_config)
    else:
        ingress = make_ingress(service_config, args)

        nginx_conf = args.config_dir + "/nginx.conf"

        ensure(args.config_dir)
        write_template(
                template=args.template,
                status=args.status_port,
                service_account=args.service_account_key,
                ingress=ingress,
                nginx_conf=nginx_conf)

        start_nginx(args.nginx, nginx_conf)

