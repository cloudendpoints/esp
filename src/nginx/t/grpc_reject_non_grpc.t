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
use strict;
use warnings;

################################################################################

use src::nginx::t::ApiManager;   # Must be first (sets up import path to the Nginx test module)
use src::nginx::t::HttpServer;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(5);

# Save service name in the service configuration protocol buffer file.
$t->write_file('service.pb.txt', ApiManager::get_bookstore_service_config);

ApiManager::write_file_expand($t, 'nginx.conf', <<"EOF");
%%TEST_GLOBALS%%
daemon off;
events {
  worker_connections 32;
}
http {
  %%TEST_GLOBALS_HTTP%%
  server_tokens off;
  server {
    listen 127.0.0.1:${NginxPort};
    server_name localhost;
    location / {
      endpoints {
        api service.pb.txt;
        %%TEST_CONFIG%%
        service_control off;
        on;
      }
      grpc_pass;
    }
  }
}
EOF

$t->run();
is($t->waitforsocket("127.0.0.1:${NginxPort}"), 1, 'Nginx socket ready.');

################################################################################

# GET call
my $response = ApiManager::http_get($NginxPort, '/test');

my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;
like($response_headers, qr/HTTP\/1\.1 404 Not Found/, 'Returned HTTP 404.');
like($response_body, qr/No HTTP backend defined for this location/, 'The error message is correct');

# POST call with content-type="application/json"
$response = ApiManager::http($NginxPort, <<EOF);
POST /test HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 20

{ "name" : "value" }
EOF

($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;
like($response_headers, qr/HTTP\/1\.1 404 Not Found/, 'Returned HTTP 404.');
like($response_body, qr/No HTTP backend defined for this location/, 'The error message is correct');

$t->stop_daemons();

################################################################################
