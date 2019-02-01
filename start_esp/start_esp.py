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
#
###############################################################################
#


import argparse
import collections
import fetch_service_config as fetch
import json
import logging
import os
import re
import sys
import textwrap
import uuid

from collections import Counter
from mako.template import Template

# Location of NGINX binary
NGINX = "/usr/sbin/nginx"
NGINX_DEBUG = "/usr/sbin/nginx-debug"

# Location of NGINX template
NGINX_CONF_TEMPLATE = "/etc/nginx/nginx-auto.conf.template"
SERVER_CONF_TEMPLATE = "/etc/nginx/server-auto.conf.template"
# Custom nginx config used by customers are hardcoded to this path
SERVER_CONF = "/etc/nginx/server_config.pb.txt"
# Default service config output directory when multiple services are proxied
SERVER_CONF_DIR = "/etc/nginx/"

# Location of generated config files
CONFIG_DIR = "/etc/nginx/endpoints"

# Protocol prefixes
GRPC_PREFIX = "grpc://"
HTTP_PREFIX = "http://"
HTTPS_PREFIX = "https://"

# Metadata service
METADATA_ADDRESS = "http://169.254.169.254"

# Management service
MANAGEMENT_ADDRESS = "https://servicemanagement.googleapis.com"

# Service management service
SERVICE_MGMT_URL_TEMPLATE = ("{}/v1/services/{}/config?configId={}")

# DNS resolver
DNS_RESOLVER = "8.8.8.8"

# Default HTTP/1.x port
DEFAULT_PORT = 8080

# Default status port
DEFAULT_STATUS_PORT = 8090

# Default backend
DEFAULT_BACKEND = "127.0.0.1:8081"

# Default rollout_strategy
DEFAULT_ROLLOUT_STRATEGY = "fixed"

# Default xff_trusted_proxy_list
DEFAULT_XFF_TRUSTED_PROXY_LIST = "0.0.0.0/0, 0::/0"

# Default PID file location (for nginx as a daemon)
DEFAULT_PID_FILE = "/var/run/nginx.pid"

# Default nginx worker_processes
DEFAULT_WORKER_PROCESSES = "1"

# Google default application credentials environment variable
GOOGLE_CREDS_KEY = "GOOGLE_APPLICATION_CREDENTIALS"

# Default access log location
DEFAULT_ACCESS_LOG = "/dev/stdout"

# Default client body buffer size
DEFAULT_CLIENT_BODY_BUFFER_SIZE = "128k"

# Default maxinum client body size
DEFAULT_CLIENT_MAX_BODY_SIZE = "32m"


Port = collections.namedtuple('Port',
        ['port', 'proto'])
Location = collections.namedtuple('Location',
        ['path', 'backends', 'proto'])
Ingress = collections.namedtuple('Ingress',
        ['ports', 'host', 'locations'])

def write_pid_file(args):
    try:
        f = open(args.pid_file, 'w+')
        f.write(str(os.getpid()))
        f.close()
    except IOError as err:
        logging.error("[ESP] Failed to save PID file: " + args.pid_file)
        logging.error(err.strerror)
        sys.exit(3)

def write_template(ingress, nginx_conf, args):
    # Load template
    try:
        template = Template(filename=args.template)
    except IOError as err:
        logging.error("[ESP] Failed to load NGINX config template. " + err.strerror)
        sys.exit(3)

    conf = template.render(
            ingress=ingress,
            pid_file=args.pid_file,
            status=args.status_port,
            service_account=args.service_account_key,
            metadata=args.metadata,
            resolver=args.dns,
            access_log=args.access_log,
            healthz=args.healthz,
            xff_trusted_proxies=args.xff_trusted_proxies,
            tls_mutual_auth=args.tls_mutual_auth,
            underscores_in_headers=args.underscores_in_headers,
            allow_invalid_headers=args.allow_invalid_headers,
            enable_websocket=args.enable_websocket,
            enable_debug=args.enable_debug,
            enable_backend_routing=args.enable_backend_routing,
            client_max_body_size=args.client_max_body_size,
            client_body_buffer_size=args.client_body_buffer_size,
            worker_processes=args.worker_processes,
            cors_preset=args.cors_preset,
            cors_allow_origin=args.cors_allow_origin,
            cors_allow_origin_regex=args.cors_allow_origin_regex,
            cors_allow_methods=args.cors_allow_methods,
            cors_allow_headers=args.cors_allow_headers,
            cors_allow_credentials=args.cors_allow_credentials,
            cors_expose_headers=args.cors_expose_headers,
            ssl_protocols=args.ssl_protocols,
            experimental_proxy_backend_host_header=args.experimental_proxy_backend_host_header,
            enable_strict_transport_security=args.enable_strict_transport_security,
            google_cloud_platform=(args.non_gcp==False))

    # Save nginx conf
    try:
        f = open(nginx_conf, 'w+')
        f.write(conf)
        f.close()
    except IOError as err:
        logging.error("[ESP] Failed to save NGINX config." + err.strerror)
        sys.exit(3)

def write_server_config_template(server_config_path, args):
    # Load template
    try:
        template = Template(filename=args.server_config_template)
    except IOError as err:
        logging.error("[ESP] Failed to load server config template. " + err.strerror)
        sys.exit(3)

    for idx, service_configs in enumerate(args.service_config_sets):
        conf = template.render(
                service_configs=service_configs,
                management=args.management,
                service_control_url_override=args.service_control_url_override,
                rollout_id=args.rollout_ids[idx],
                rollout_strategy=args.rollout_strategy,
                always_print_primitive_fields=args.transcoding_always_print_primitive_fields,
                client_ip_header=args.client_ip_header,
                client_ip_position=args.client_ip_position,
                rewrite_rules=args.rewrite,
                disable_cloud_trace_auto_sampling=args.disable_cloud_trace_auto_sampling,
                cloud_trace_url_override=args.cloud_trace_url_override,
                log_request_headers=args.request_headers,
                log_response_headers=args.response_headers,
                metadata_attributes=args.metadata_attributes)

        server_config_file = server_config_path
        if server_config_file.endswith('/'):
            server_config_file = os.path.join(server_config_path, args.services[idx] + '_server_config.txt')

        # Save server conf
        try:
            f = open(server_config_file, 'w+')
            f.write(conf)
            f.close()
        except IOError as err:
            logging.error("[ESP] Failed to save server config." + err.strerror)
            sys.exit(3)

def ensure(config_dir):
    if not os.path.exists(config_dir):
        try:
            os.makedirs(config_dir)
        except OSError as exc:
            logging.error("[ESP] Cannot create config directory.")
            sys.exit(3)


def assert_file_exists(fl):
    if not os.path.exists(fl):
        logging.error("[ESP] Cannot find the specified file " + fl)
        sys.exit(3)


def start_nginx(nginx, nginx_conf):
    try:
        # Control is relinquished to nginx process after this line
        os.execv(nginx, ['nginx', '-p', '/usr', '-c', nginx_conf])
    except OSError as err:
        logging.error("[ESP] Failed to launch NGINX: " + nginx)
        logging.error(err.strerror)
        sys.exit(3)

def fetch_and_save_service_config_url(config_dir, token, service_mgmt_url, filename):
    try:
        # download service config
        config = fetch.fetch_service_json(service_mgmt_url, token)

        # Save service json for ESP
        service_config = config_dir + "/" + filename

        try:
            f = open(service_config, 'w+')
            json.dump(config, f, sort_keys=True, indent=2,
                      separators=(',', ': '))
            f.close()
        except IOError as err:
            logging.error("[ESP] Cannot save service config." + err.strerror)
            sys.exit(3)

    except fetch.FetchError as err:
        logging.error(err.message)
        sys.exit(err.code)

def fetch_and_save_service_config(management, service, config_dir, token, version, filename):
    try:
        # build request url
        service_mgmt_url = SERVICE_MGMT_URL_TEMPLATE.format(management,
                                                            service,
                                                            version)
        # Validate service config if we have service name and version
        logging.info("Fetching the service configuration "\
                     "from the service management service")
        fetch_and_save_service_config_url(config_dir, token, service_mgmt_url, filename)

    except fetch.FetchError as err:
        logging.error(err.message)
        sys.exit(err.code)

# config_id might have invalid character for file name.
def generate_service_config_filename(version):
    return str(uuid.uuid5(uuid.NAMESPACE_DNS, str(version)))

# parse xff_trusted_proxy_list
def handle_xff_trusted_proxies(args):
    args.xff_trusted_proxies = []
    if args.xff_trusted_proxy_list is not None:
        for proxy in args.xff_trusted_proxy_list.split(","):
            proxy = proxy.strip()
            if proxy:
                args.xff_trusted_proxies.append(proxy)

# parse http headers list
def handle_http_headers(args):
    args.request_headers = []
    if args.log_request_headers is not None:
        for header in args.log_request_headers.split(","):
            header = header.strip()
            if header:
                args.request_headers.append(header)

    args.response_headers = []
    if args.log_response_headers is not None:
        for header in args.log_response_headers.split(","):
            header = header.strip()
            if header:
                args.response_headers.append(header)

def fetch_service_config(args):
    args.service_config_sets = []
    args.rollout_ids = []

    try:
        # Check service_account_key and non_gcp
        if args.non_gcp and args.service_account_key is None:
            logging.error("[ESP] If --non_gcp is specified, --service_account_key has to be specified");
            sys.exit(3)

        # Get the access token
        if args.service_account_key is None:
            logging.info("Fetching an access token from the metadata service")
            token = fetch.fetch_access_token(args.metadata)
        else:
            token = fetch.make_access_token(args.service_account_key)

        if args.service_config_url is not None:
            # Set the file name to "service.json", if either service
            # config url or version is specified for backward compatibility
            filename = "service.json"
            fetch_and_save_service_config_url(args.config_dir, token, args.service_config_url, filename)
            args.service_config_sets.append({})
            args.service_config_sets[0][args.config_dir + "/" + filename] = 100;
        else:
            # fetch service name, if not specified
            if (args.service is None or not args.service.strip()) and args.check_metadata:
                logging.info(
                    "Fetching the service name from the metadata service")
                args.service = fetch.fetch_service_name(args.metadata)

            # if service name is not specified, display error message and exit
            if args.service is None:
                if args.check_metadata:
                    logging.error("[ESP] Unable to fetch service name from the metadata service");
                else:
                    logging.error("[ESP] Service name is not specified");
                sys.exit(3)

            # fetch service config rollout strategy from metadata, if not specified
            if (args.rollout_strategy is None or not args.rollout_strategy.strip()) and args.check_metadata:
                logging.info(
                    "Fetching the service config rollout strategy from the metadata service")
                args.rollout_strategy = \
                    fetch.fetch_service_config_rollout_strategy(args.metadata);

            if args.rollout_strategy is None or not args.rollout_strategy.strip():
                args.rollout_strategy = DEFAULT_ROLLOUT_STRATEGY

            # fetch service config ID, if not specified
            if (args.version is None or not args.version.strip()) and args.check_metadata:
                logging.info("Fetching the service config ID "\
                             "from the metadata service")
                args.version = fetch.fetch_service_config_id(args.metadata)

            # Fetch api version from latest successful rollouts
            if args.version is None or not args.version.strip():
                services = args.service.split('|')
                args.services = services
                for idx, service in enumerate(services):
                    logging.info(
                        "Fetching the service config ID from the rollouts service")
                    rollout = fetch.fetch_latest_rollout(args.management,
                                                         service, token)
                    args.rollout_ids.append(rollout["rolloutId"])
                    args.service_config_sets.append({})
                    for version, percentage in rollout["trafficPercentStrategy"]["percentages"].iteritems():
                        filename = generate_service_config_filename(version)
                        fetch_and_save_service_config(args.management, service, args.config_dir, token, version, filename)
                        args.service_config_sets[idx][args.config_dir + "/" + filename] = percentage;
            else:
                # Set the file name to "service.json", if either service
                # config url or version is specified for backward compatibility
                filename = "service.json"
                fetch_and_save_service_config(args.management, args.service, args.config_dir, token, args.version, filename)
                args.service_config_sets.append({})
                args.service_config_sets[0][args.config_dir + "/" + filename] = 100;

        if not args.rollout_ids:
            args.rollout_ids.append("")

    except fetch.FetchError as err:
        logging.error(err.message)
        sys.exit(err.code)


def make_ingress(args):
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
            logging.error("[ESP] Port " + str(shared_port) + " is used more than once.")
            sys.exit(2)

    if args.http_port is not None:
        ports.append(Port(args.http_port, "http"))
    if args.http2_port is not None:
        ports.append(Port(args.http2_port, "http2"))
    if args.ssl_port is not None:
        ports.append(Port(args.ssl_port, "ssl"))

    if args.backend.startswith(GRPC_PREFIX):
        proto = "grpc"
        backends = [args.backend[len(GRPC_PREFIX):]]
    elif args.backend.startswith(HTTP_PREFIX):
        proto = "http"
        backends = [args.backend[len(HTTP_PREFIX):]]
    elif args.backend.startswith(HTTPS_PREFIX):
        proto = "https"
        backend = args.backend[len(HTTPS_PREFIX):]
        if not re.search(r':[0-9]+$', backend):
            backend = backend + ':443'
        backends = [backend]
    else:
        proto = "http"
        backends = [args.backend]

    locations = [Location(
            path='/',
            backends=backends,
            proto=proto)]

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
    parser = ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,
            description = '''
ESP start-up script. This script fetches the service configuration from the
service management service and configures ESP to expose the specified ports and
proxy requests to the specified backend.

The service name and config ID are optional. If not supplied, the script
fetches the service name and the config ID from the metadata service as
attributes "{service_name}" and "{service_config_id}".

ESP relies on the metadata service to fetch access tokens for Google services.
If you deploy ESP outside of Google Cloud environment, you need to provide a
service account credentials file by setting {creds_key}
environment variable or by passing "-k" flag to this script.

If a custom nginx config file is provided ("-n" flag), the script launches ESP
with the provided config file. Otherwise, the script uses the exposed ports
("-p", "-P", "-S", "-N" flags) and the backend ("-a" flag) to generate an nginx
config file.'''.format(
        service_name = fetch._METADATA_SERVICE_NAME,
        service_config_id = fetch._METADATA_SERVICE_CONFIG_ID,
        creds_key = GOOGLE_CREDS_KEY
    ))

    parser.add_argument('-k', '--service_account_key', help=''' Use the service
    account key JSON file to access the service control and the service
    management.  You can also set {creds_key} environment
    variable to the location of the service account credentials JSON file. If
    the option is omitted, ESP contacts the metadata service to fetch an access
    token.  '''.format(creds_key = GOOGLE_CREDS_KEY))

    parser.add_argument('-s', '--service', help=''' Set the name of the
    Endpoints service.  If omitted and -c not specified, ESP contacts the
    metadata service to fetch the service name. When --experimental_enable_multiple_api_configs
    is enabled, to specify multiple services,
    separate them by the pipe character (|) and enclose the argument
    value in quotes, e.g.,
    --service="svc1.example.com|svc2.example.com" ''')

    parser.add_argument('-v', '--version', help=''' Set the service config ID of
    the Endpoints service.  If omitted and -c not specified, ESP contacts the
    metadata service to fetch the service config ID.  ''')

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
    secure connections. Requires the certificate and key files
    /etc/nginx/ssl/nginx.crt and /etc/nginx/ssl/nginx.key''')

    parser.add_argument('-N', '--status_port', default=DEFAULT_STATUS_PORT,
    type=int, help=''' Change the ESP status port. Status information is
    available at /endpoints_status location over HTTP/1.x. Default value:
    {port}.'''.format(port=DEFAULT_STATUS_PORT))

    parser.add_argument('-a', '--backend', default=DEFAULT_BACKEND, help='''
    Change the application server address to which ESP proxies the requests.
    Default value: {backend}. For HTTPS backends, please use "https://" prefix,
    e.g. https://127.0.0.1:8081. For HTTP/1.x backends, prefix "http://" is
    optional. For GRPC backends, please use "grpc://" prefix,
    e.g. grpc://127.0.0.1:8081.'''.format(backend=DEFAULT_BACKEND))

    parser.add_argument('-t', '--tls_mutual_auth', action='store_true', help='''
    Enable TLS mutual authentication for HTTPS backends.
    Default value: Not enabled. Please provide the certificate and key files
    /etc/nginx/ssl/backend.crt and /etc/nginx/ssl/backend.key.''')

    parser.add_argument('-c', '--service_config_url', default=None, help='''
    Use the specified URL to fetch the service configuration instead of using
    the default URL template
    {template}.'''.format(template=SERVICE_MGMT_URL_TEMPLATE))

    parser.add_argument('-z', '--healthz', default=None, help='''Define a
    health checking endpoint on the same ports as the application backend. For
    example, "-z healthz" makes ESP return code 200 for location "/healthz",
    instead of forwarding the request to the backend.  Default: not used.''')

    parser.add_argument('-R', '--rollout_strategy',
        default=None,
        help='''The service config rollout strategy, [fixed|managed],
        Default value: {strategy}'''.format(strategy=DEFAULT_ROLLOUT_STRATEGY),
        choices=['fixed', 'managed'])

    parser.add_argument('-x', '--xff_trusted_proxy_list',
        default=DEFAULT_XFF_TRUSTED_PROXY_LIST,
        help='''Comma separated list of trusted proxy for X-Forwarded-For
        header, Default value: {xff_trusted_proxy_list}'''.
        format(xff_trusted_proxy_list=DEFAULT_XFF_TRUSTED_PROXY_LIST))

    parser.add_argument('--experimental_proxy_backend_host_header', default=None,
        help='''Define the Host header value that overrides the incoming Host
        header for upstream request.''')

    parser.add_argument('--check_metadata', action='store_true',
        help='''Enable fetching access token, service name, service config ID
        and rollout strategy from the metadata service''')

    parser.add_argument('--underscores_in_headers', action='store_true',
        help='''Allow headers contain underscores to pass through by setting
        "underscores_in_headers on;" directive.
        ''')

    parser.add_argument('--allow_invalid_headers', action='store_true',
        help='''Allow "invalid" headers by adding "ignore_invalid_headers off;"
        directive. This is required to support all legal characters specified
        in RFC 7230.
        ''')

    parser.add_argument('--enable_websocket', action='store_true',
        help='''Enable nginx WebSocket support.
        ''')

    parser.add_argument('--enable_strict_transport_security', action='store_true',
        help='''Enable HSTS (HTTP Strict Transport Security).
        ''')

    parser.add_argument('--enable_debug', action='store_true',
        help='''Run debug Nginx binary with debug trace.
        ''')

    parser.add_argument('--generate_self_signed_cert', action='store_true',
        help='''Generate a self-signed certificate and key at start, then
        store them in /etc/nginx/ssl/nginx.crt and /etc/nginx/ssl/nginx.key.
        This is useful when only a random self-sign cert is needed to serve
        HTTPS requests. Generated certificate will have Common Name
        "localhost" and valid for 10 years.
        ''')

    parser.add_argument('--client_max_body_size', default=DEFAULT_CLIENT_MAX_BODY_SIZE, help='''
    Sets the maximum allowed size of the client request body, specified
    in the "Content-Length" request header field. If the size in a request
    exceeds the configured value, the 413 (Request Entity Too Large) error
    is returned to the client. Please be aware that browsers cannot correctly
    display this error. Setting size to 0 disables checking of client request
    body size.''')

    parser.add_argument('--client_body_buffer_size', default=DEFAULT_CLIENT_BODY_BUFFER_SIZE, help='''
    Sets buffer size for reading client request body. In case the request
    body is larger than the buffer, the whole body or only its part is
    written to a temporary file.''')

    parser.add_argument('--rewrite', action='append', help=
    '''Internally redirect the request uri with a pair of pattern and
    replacement. Pattern and replacement should be separated by whitespace.
    If the request uri matches perl compatible regular expression,
    then the request uri will be replaced with the replacement.
    pattern and replacement follow the rewrite function of Module
    ngx_http_rewrite_module except flag.
    http://nginx.org/en/docs/http/ngx_http_rewrite_module.html#rewrite
    The "rewrite" argument can be repeat multiple times. Rewrite rules are
    executed sequentially in the order of arguments.
    ex.
    --rewrite "/apis/shelves\\\\?id=(.*)&key=(.*) /shelves/\$1?key=\$2"
    --rewrite "^/api/v1/view/(.*) /view/\$1"
    ''')

    parser.add_argument('--worker_processes', default=DEFAULT_WORKER_PROCESSES,
    help='''Value for nginx "worker_processes". Each worker is a single process
    with no additional threads, so scale this if you will receive more load
    than a single CPU can handle. Use `auto` to automatically set to the number
    of CPUs available, but be aware that containers may be limited to less than
    that of their host. Also, the ESP cache to Service Control is per-worker,
    so keep this value as low as possible.
    ''')

    # Specify a custom service.json path.
    # If this is specified, service json will not be fetched.
    parser.add_argument('--service_json_path',
        default=None,
        help=argparse.SUPPRESS)

    # Customize metadata service url prefix.
    parser.add_argument('-m', '--metadata',
        default=METADATA_ADDRESS,
        help=argparse.SUPPRESS)

    # Customize management service url prefix.
    parser.add_argument('-g', '--management',
        default=MANAGEMENT_ADDRESS,
        help=argparse.SUPPRESS)

    # Customize servicecontrol url prefix.
    parser.add_argument('--service_control_url_override',
        default=None,
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

    # nginx.conf template
    parser.add_argument('--server_config_template',
        default=SERVER_CONF_TEMPLATE,
        help=argparse.SUPPRESS)

    # nginx binary location
    parser.add_argument('--nginx',
        default=NGINX,
        help=argparse.SUPPRESS)

    # nginx_debug binary location
    parser.add_argument('--nginx_debug',
        default=NGINX_DEBUG,
        help=argparse.SUPPRESS)

    # Address of the DNS resolver used by nginx http.cc
    parser.add_argument('--dns',
        default=DNS_RESOLVER,
        help=argparse.SUPPRESS)

    # Access log destination. Use special value 'off' to disable.
    parser.add_argument('--access_log',
        default=DEFAULT_ACCESS_LOG,
        help=argparse.SUPPRESS)

    # PID file location.
    parser.add_argument('--pid_file',
        default=DEFAULT_PID_FILE,
        help=argparse.SUPPRESS)

    # always_print_primitive_fields.
    parser.add_argument('--transcoding_always_print_primitive_fields',
        action='store_true',
        help=argparse.SUPPRESS)

    parser.add_argument('--client_ip_header', default=None, help='''
    Defines the HTTP header name to extract client IP address.''')

    parser.add_argument('--client_ip_position', default=0, help='''
    Defines the position of the client IP address. The default value is 0.
    The index usage is the same as the array index in many languages,
    such as Python. This flag is only applied when --client_ip_header is
    specified.''')

    # CORS presets
    parser.add_argument('--cors_preset',
        default=None,
        help='''
        Enables setting of CORS headers. This is useful when using a GRPC
        backend, since a GRPC backend cannot set CORS headers.
        Specify one of available presets to configure CORS response headers
        in nginx. Defaults to no preset and therefore no CORS response
        headers. If no preset is suitable for the use case, use the
        --nginx_config arg to use a custom nginx config file.
        Available presets:
        - basic - Assumes all location paths have the same CORS policy.
          Responds to preflight OPTIONS requests with an empty 204, and the
          results of preflight are allowed to be cached for up to 20 days
          (1728000 seconds). See descriptions for args --cors_allow_origin,
          --cors_allow_methods, --cors_allow_headers, --cors_expose_headers,
          --cors_allow_credentials for more granular configurations.
        - cors_with_regex - Same as basic preset, except that specifying
          allowed origins in regular expression. See descriptions for args
          --cors_allow_origin_regex, --cors_allow_methods,
          --cors_allow_headers, --cors_expose_headers, --cors_allow_credentials
          for more granular configurations.
        ''')
    parser.add_argument('--cors_allow_origin',
        default='*',
        help='''
        Only works when --cors_preset is 'basic'. Configures the CORS header
        Access-Control-Allow-Origin. Defaults to "*" which allows all origins.
        ''')
    parser.add_argument('--cors_allow_origin_regex',
        default='',
        help='''
        Only works when --cors_preset is 'cors_with_regex'. Configures the
        whitelists of CORS header Access-Control-Allow-Origin with regular
        expression.
        ''')
    parser.add_argument('--cors_allow_methods',
        default='GET, POST, PUT, PATCH, DELETE, OPTIONS',
        help='''
        Only works when --cors_preset is in use. Configures the CORS header
        Access-Control-Allow-Methods. Defaults to allow common HTTP
        methods.
        ''')
    parser.add_argument('--cors_allow_headers',
        default='DNT,User-Agent,X-Requested-With,If-Modified-Since,Cache-Control,Content-Type,Range,Authorization',
        help='''
        Only works when --cors_preset is in use. Configures the CORS header
        Access-Control-Allow-Headers. Defaults to allow common HTTP
        headers.
        ''')
    parser.add_argument('--cors_allow_credentials', action='store_true',
        help='''
        Only works when --cors_preset is in use. Enable the CORS header
        Access-Control-Allow-Credentials. By default, this header is disabled.
        ''')
    parser.add_argument('--cors_expose_headers',
        default='Content-Length,Content-Range',
        help='''
        Only works when --cors_preset is in use. Configures the CORS header
        Access-Control-Expose-Headers. Defaults to allow common response headers.
        ''')
    parser.add_argument('--non_gcp', action='store_true',
        help='''
        By default, ESP tries to talk to GCP metadata server to get VM
        location in the first few requests. setting this flag to true to skip
        this step.
        ''')
    parser.add_argument('--disable_cloud_trace_auto_sampling', action='store_true',
        help='''
        Disable cloud trace auto sampling. By default, 1 request out of every
        1000 or 1 request out of every 10 seconds is enabled with cloud trace.
        Set this flag to false to disable such auto sampling. Cloud trace can
        still be enabled from request HTTP headers with trace context regardless
        this flag value.
        ''')
    parser.add_argument('--ssl_protocols',
        default=None, action='append', help='''
        Enable the specified SSL protocols. Please refer to
        https://nginx.org/en/docs/http/ngx_http_ssl_module.html#ssl_protocols.
        The "ssl_protocols" argument can be repeated multiple times to specify multiple
        SSL protocols (e.g., --ssl_protocols=TLSv1.1 --ssl_protocols=TLSv1.2).
        ''')
    parser.add_argument('--generate_config_file_only', action='store_true',
        help='''Only generate the nginx config file without running ESP. This option is
        for testing that the generated nginx config file is as expected.
        ''')
    parser.add_argument('--server_config_generation_path',
        default=None, help='''
        Define where to write the server configuration file(s). For a single server
        configuration file, this must be a file name.
        When --experimental_enable_multiple_api_configs is enabled, to write multiple server
        configuration files, this must be a directory path that ends with a '/'.
        When --generate_config_file_only is used but
        --server_config_generation_path is absent, the server configuration file generation
        is skipped.
        ''')
    parser.add_argument('--experimental_enable_multiple_api_configs', action='store_true',
                        help='''
        Enable an experimental feature that proxies multiple Endpoints services.
        By default, this feature is disabled.
        ''')

    # Customize cloudtrace service url prefix.
    parser.add_argument('--cloud_trace_url_override',
        default=None,
        help=argparse.SUPPRESS)

    parser.add_argument('--log_request_headers', default=None, help='''
        Log corresponding request headers into Google cloud console through
        service control, separated by comma. Example, when
        --log_request_headers=foo,bar, endpoint log will have
        request_headers: foo=foo_value;bar=bar_value if values are available;
        ''')

    parser.add_argument('--log_response_headers', default=None, help='''
        Log corresponding response headers into Google cloud console through
        service control, separated by comma. Example, when
        --log_response_headers=foo,bar, endpoint log will have
        response_headers: foo=foo_value;bar=bar_value if values are available;
        ''')

    parser.add_argument('--enable_backend_routing',
        action='store_true', help='''
        Enables the nginx proxy to route requests according to the `x-google-backend` or
        `backend` configuration. This flag conflicts with a few of other flags.
        ''')

    return parser

# Check whether there are conflict flags. If so, return the error string. Otherwise returns None.
# This function also changes some default flag value.
def enforce_conflict_args(args):
    if args.generate_config_file_only:
        # this is for test purpose.
        if args.server_config_generation_path:
            return None
        if args.nginx_config:
            return "--nginx_config is not allowed when --generate_config_file_only"

    if args.enable_backend_routing:
        if args.cloud_trace_url_override:
            return "Flag --enable_backend_routing cannot be used together with --cloud_trace_url_override."
        if args.experimental_enable_multiple_api_configs:
            return "Flag --enable_backend_routing cannot be used together with --experimental_enable_multiple_api_configs."
        if args.disable_cloud_trace_auto_sampling:
            return "Flag --enable_backend_routing cannot be used together with --disable_cloud_trace_auto_sampling."
        if args.non_gcp:
            return "Flag --enable_backend_routing cannot be used together with --non_gcp."
        if args.transcoding_always_print_primitive_fields:
            return "Flag --enable_backend_routing cannot be used together with --transcoding_always_print_primitive_fields."
        if args.pid_file != DEFAULT_PID_FILE:
            return "Flag --enable_backend_routing cannot be used together with --pid_file."
        if args.access_log != DEFAULT_ACCESS_LOG:
            return "Flag --enable_backend_routing cannot be used together with --access_log."
        if args.nginx_debug != NGINX_DEBUG:
            return "Flag --enable_backend_routing cannot be used together with --nginx_debug."
        if args.nginx != NGINX:
            return "Flag --enable_backend_routing cannot be used together with --nginx."
        if args.server_config_template != SERVER_CONF_TEMPLATE:
            return "Flag --enable_backend_routing cannot be used together with --server_config_template."
        if args.template != NGINX_CONF_TEMPLATE:
            return "Flag --enable_backend_routing cannot be used together with --template."
        if args.config_dir != CONFIG_DIR:
            return "Flag --enable_backend_routing cannot be used together with --config_dir."
        if args.service_control_url_override:
            return "Flag --enable_backend_routing cannot be used together with --service_control_url_override."
        if args.metadata != METADATA_ADDRESS:
            return "Flag --enable_backend_routing cannot be used together with --metadata."
        if args.service_json_path:
            return "Flag --enable_backend_routing cannot be used together with --service_json_path."
        if args.worker_processes != DEFAULT_WORKER_PROCESSES:
            return "Flag --enable_backend_routing cannot be used together with --worker_processes."
        if args.rewrite:
            return "Flag --enable_backend_routing cannot be used together with --rewrite."
        if args.client_body_buffer_size != DEFAULT_CLIENT_BODY_BUFFER_SIZE:
            return "Flag --enable_backend_routing cannot be used together with --client_body_buffer_size."
        if args.client_max_body_size != DEFAULT_CLIENT_MAX_BODY_SIZE:
            return "Flag --enable_backend_routing cannot be used together with --client_max_body_size."
        if args.generate_self_signed_cert:
            return "Flag --enable_backend_routing cannot be used together with --generate_self_signed_cert."
        if args.enable_strict_transport_security:
            return "Flag --enable_backend_routing cannot be used together with --enable_strict_transport_security."
        if args.enable_websocket:
            return "Flag --enable_backend_routing cannot be used together with --enable_websocket."
        if args.allow_invalid_headers:
            return "Flag --enable_backend_routing cannot be used together with --allow_invalid_headers."
        if args.underscores_in_headers:
            return "Flag --enable_backend_routing cannot be used together with --underscores_in_headers."
        if args.experimental_proxy_backend_host_header:
            return "Flag --enable_backend_routing cannot be used together with --experimental_proxy_backend_host_header."
        if args.xff_trusted_proxy_list != DEFAULT_XFF_TRUSTED_PROXY_LIST:
            return "Flag --enable_backend_routing cannot be used together with -x or --xff_trusted_proxy_list."
        if args.healthz:
            return "Flag --enable_backend_routing cannot be used together with -z or --healthz."
        if args.tls_mutual_auth:
            return "Flag --enable_backend_routing cannot be used together with -t or --tls_mutual_auth."
        if args.backend != DEFAULT_BACKEND:
            return "Flag --enable_backend_routing cannot be used together with -a or --backend."
        if args.status_port != DEFAULT_STATUS_PORT:
            return "Flag --enable_backend_routing cannot be used together with -N or --status_port."
        if args.nginx_config:
            return "Flag --enable_backend_routing cannot be used together with -n or --nginx_config."

        # When --enable_backend_routing is specified, set some default value to some of its conflicting flags.
        args.disable_cloud_trace_auto_sampling = True
        args.access_log = 'off'
    return None

if __name__ == '__main__':
    parser = make_argparser()
    args = parser.parse_args()
    logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.INFO)

    check_conflict_result = enforce_conflict_args(args)
    if check_conflict_result:
        logging.error(check_conflict_result)
        sys.exit(3)

    if args.service and '|' in args.service:
        if args.experimental_enable_multiple_api_configs == False:
            logging.error("[ESP] The flag --experimental_enable_multiple_api_configs must be enabled when --service specifies multiple services")
            sys.exit(3)
        if args.version:
            logging.error("[ESP] --version is not allowed when --service specifies multiple services")
            sys.exit(3)
        if args.server_config_generation_path and not args.server_config_generation_path.endswith('/'):
            logging.error("[ESP] --server_config_generation_path must end with / when --service specifies multiple services")
            sys.exit(3)

    # Set credentials file from the environment variable
    if args.service_account_key is None:
        if GOOGLE_CREDS_KEY in os.environ:
            args.service_account_key = os.environ[GOOGLE_CREDS_KEY]

    # Write pid file for the supervising process
    write_pid_file(args)

    # Handles IP addresses of trusted proxies
    handle_xff_trusted_proxies(args)

    # Handles http headers
    handle_http_headers(args)

    # Get service config
    if args.service_json_path:
        args.rollout_ids = ['']
        assert_file_exists(args.service_json_path)
        args.service_config_sets = [{args.service_json_path: 100}]
    else:
        # Fetch service config and place it in the standard location
        ensure(args.config_dir)
        if not args.generate_config_file_only:
            fetch_service_config(args)

    # Generate server_config
    args.metadata_attributes = fetch.fetch_metadata_attributes(args.metadata)
    if args.generate_config_file_only:
        # When generate_config_file_only, metadata_attributes is set as empty
        # to have consistent test results on local bazel test and jenkins test
        # environments.
        args.metadata_attributes = None
        if args.server_config_generation_path is None:
            logging.error("[ESP] when --generate_config_file_only, must specify --server_config_generation_path")
            sys.exit(3)
        else:
            write_server_config_template(args.server_config_generation_path, args)
    else:
        server_config_path = SERVER_CONF
        if args.service and '|' in args.service:
            server_config_path = SERVER_CONF_DIR
        if args.server_config_generation_path:
            server_config_path = args.server_config_generation_path
        write_server_config_template(server_config_path, args)

    # Generate nginx config if not specified
    nginx_conf = args.nginx_config
    if nginx_conf is None:
        ingress = make_ingress(args)
        nginx_conf = args.config_dir + "/nginx.conf"
        ensure(args.config_dir)
        write_template(ingress, nginx_conf, args)

    if args.generate_config_file_only:
        exit(0)

    # Generate self-signed cert if needed
    if args.generate_self_signed_cert:
        if not os.path.exists("/etc/nginx/ssl"):
            os.makedirs("/etc/nginx/ssl")
        logging.info("Generating self-signed certificate...")
        os.system(("openssl req -x509 -newkey rsa:2048"
                   " -keyout /etc/nginx/ssl/nginx.key -nodes"
                   " -out /etc/nginx/ssl/nginx.crt"
                   ' -days 3650 -subj "/CN=localhost"'))

    # Start NGINX
    nginx_bin = args.nginx
    if args.enable_debug:
      nginx_bin = args.nginx_debug
    start_nginx(nginx_bin, nginx_conf)
