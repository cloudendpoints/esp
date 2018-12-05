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
use strict;
use warnings;

################################################################################

use src::nginx::t::ApiManager;   # Must be first (sets up import path to the Nginx test module)
use src::nginx::t::HttpServer;
use src::nginx::t::ServiceControl;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(18);

# Save service name in the service configuration protocol buffer file.
my $config = ApiManager::get_bookstore_service_config_allow_some_unregistered . <<"EOF";
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF
$t->write_file('service.pb.txt', $config);

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
        on;
      }
      proxy_pass http://127.0.0.1:${BackendPort};
    }
  }
}
EOF

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');

$t->run();

################################################################################

# Use different api_key so service_control will not use its cache.
my $response1 = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key1');
my $response2 = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key2');

$t->stop_daemons();

like($response1, qr/HTTP\/1\.1 403 Forbidden/, 'Response1 returned HTTP 403.');
like($response1, qr/content-type: application\/json/i,
    'Forbidden has application/json body.');
like($response1, qr/API endpoints-test.cloudendpointsapis.com is not enabled for the project/i,
    "Error body contains 'activation error'.");

like($response2, qr/HTTP\/1\.1 400 Bad Request/, 'Response2 returned HTTP 400.');
like($response2, qr/content-type: application\/json/i,
    'Forbidden has application/json body.');
like($response2, qr/API key not valid/i,
    "Error body contains 'API key error'.");


my $bookstore_requests = $t->read_file('bookstore.log');
is($bookstore_requests, '', 'Request did not reach the backend.');

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 2, 'Service control received 2 request.');

# :check #1
my $r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':check 1 was a post');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check',
    ':check 1 uri is correct');
is($r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}", ':check 1 Host header was set.');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', ':check 1 Content-Type is protocol buffer.');

# :check #2
$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':check 2 was a post');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check',
    ':check 2 uri is correct');
is($r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}", ':check 2 Host header was set.');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', ':check 2 Content-Type is protocol buffer.');

################################################################################

sub bookstore {
    my ($t, $port, $file) = @_;
    my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
        or die "Can't create test server socket: $!\n";
    local $SIG{PIPE} = 'IGNORE';

    # Do not initialize any server state, requests won't reach backend anyway.

    $server->run();
}

sub servicecontrol {
    my ($t, $port, $file) = @_;
    my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
        or die "Can't create test server socket: $!\n";
    local $SIG{PIPE} = 'IGNORE';

    # This is normal 200 HTTP response with error in CheckResponse Json
    my $proto_response1 = ServiceControl::convert_proto(<<'EOF', 'check_response', 'binary');
{
  "operationId": "ListShelves:7b3f4c4f-f29c-4391-b35e-0a676427fec8",
  "checkErrors": [
    {
      "code": "SERVICE_NOT_ACTIVATED",
      "detail": "Project has not activated the endpoints-test.cloudendpointsapis.com API."
    }
  ]
}
EOF

    $server->on('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', <<'EOF' . $proto_response1);
HTTP/1.1 200 OK
Connection: close

EOF

    # This is normal 200 HTTP response with error in api key
    my $proto_response2 = ServiceControl::convert_proto(<<'EOF', 'check_response', 'binary');
{
  "operationId": "ListShelves:7b3f4c4f-f29c-4391-b35e-0a676427fec9",
  "checkErrors": [
    {
      "code": "API_KEY_INVALID",
      "detail": "API is not valid."
    }
  ]
}
EOF

    $server->on('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', <<'EOF' . $proto_response2);
HTTP/1.1 200 OK
Connection: close

EOF



    $server->run();
}

################################################################################