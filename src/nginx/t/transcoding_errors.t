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
my $ServiceControlPort = ApiManager::pick_port();
my $GrpcServerPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(30);

$t->write_file('service.pb.txt',
    ApiManager::get_transcoding_test_service_config(
        'endpoints-transcoding-test.cloudendpointsapis.com',
        "http://127.0.0.1:${ServiceControlPort}"));

ApiManager::write_file_expand($t, 'nginx.conf', <<EOF);
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
      grpc_pass 127.0.0.1:${GrpcServerPort};
    }
  }
}
EOF

$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'servicecontrol.log');

is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, "Service control socket ready.");
$t->run();
is($t->waitforsocket("127.0.0.1:${NginxPort}"), 1, "Nginx socket ready.");

################################################################################

# 1. Check fails
my $response = ApiManager::http_get($NginxPort,'/shelves?key=api-key-1');

like($response, qr/HTTP\/1\.1 403 Forbidden/, 'Got a 403.');
like($response, qr/Content-Type: application\/json/i, 'Content-type is application/json');
like($response,
    qr/API endpoints-transcoding-test.cloudendpointsapis.com is not enabled for the project./i,
    "the message from service-control was propogated.");


# 2. The backend is not running yet. We should get 503.
$response = ApiManager::http_get($NginxPort,'/shelves?key=api-key-2');

like($response, qr/HTTP\/1\.1 503 Service Temporarily Unavailable/, 'Got a 503.');
like($response, qr/Content-Type: application\/json/i, 'Content-type is application/json');
like($response, qr/upstream backend unavailable/i, "The unavailable message is correct.");

################################################################################

# Start the backend to continue with the rest of the tests
ApiManager::run_transcoding_test_server($t, 'server.log', "127.0.0.1:${GrpcServerPort}");
is($t->waitforsocket("127.0.0.1:${GrpcServerPort}"), 1, "GRPC test server socket ready.");

################################################################################

# Try to make a successful call before continuing with the testing as sometimes
# the first call after bringing up the backend fails.  Not sure what's the
# reason, but one possibility is that the gRPC client in ESP caches the
# "unavailable" status of the backend for some time. So even if the backend is
# up right now as the last time we called the backend it was unavailable, it
# decides to return unavailable this time as well.

my $try = 0;
my $max_tries = 5;
while ($try < $max_tries) {
    $response = ApiManager::http_get($NginxPort, '/shelves?key=test-api-key');
    if ($response =~ qr/HTTP\/1.. 200 OK/) {
        last;
    } else {
        # sleep 1 second before trying again
        sleep(1);
        $try++;
    }
}

if ($try == $max_tries) {
    fail('Failed to make a successful call after starting the backend.');
}

################################################################################

# 3. Error propogated from the backend - shelf doesn't exist
$response = ApiManager::http_get($NginxPort,'/shelves/100?key=api-key-3');

like($response, qr/HTTP\/1\.1 404 Not Found/, 'Got a 404.');
like($response, qr/Content-Type: application\/json/i, 'Content-type is application/json');
# This error message is from grpc-status-details-bin header from gRPC backend.
like($response, qr/ShelfNotFoundDetail/i, "the message from the backend was propogated.");

# 4. Calling a method that does not exist
$response = ApiManager::http_get($NginxPort,'/non-existing-method?key=api-key-4');

like($response, qr/HTTP\/1\.1 404 Not Found/, 'Got a 404.');
like($response, qr/Content-Type: application\/json/i, 'Content-type is application/json');
like($response, qr/Method does not exist/i, "the message from the backend was propogated.");

# 5. Posting invalid JSON - no braces
$response = ApiManager::http($NginxPort,<<EOF);
POST /shelves?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 16

NOT_A_VALID_JSON
EOF

like($response, qr/HTTP\/1\.1 400 Bad Request/, 'Got a 400.');
like($response, qr/Content-Type: application\/json/i, 'Content-type is application/json');
like($response, qr/Unexpected token.*NOT_A_VALID_JSON.*/i, "Got the unexpected token message");

# 6. Posting invalid JSON - mismatched {
$response = ApiManager::http($NginxPort,<<EOF);
POST /shelves?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 22

{ "theme" : "Children"
EOF

like($response, qr/HTTP\/1\.1 400 Bad Request/, 'Got a 400.');
like($response, qr/Content-Type: application\/json/i, 'Content-type is application/json');
like($response, qr/Unexpected end of string.*/i, "Got the mismatched { message");

# 7. Posting invalid JSON - extra characters
$response = ApiManager::http($NginxPort,<<EOF);
POST /shelves?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 30

{ "theme" : "Children" } EXTRA
EOF

like($response, qr/HTTP\/1\.1 400 Bad Request/, 'Got a 400.');
like($response, qr/Content-Type: application\/json/i, 'Content-type is application/json');
like($response, qr/Parsing terminated before end of input./i, "Got the extra characters message");

# 8. Posting invalid JSON - no colon
$response = ApiManager::http($NginxPort,<<EOF);
POST /shelves?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 22

{ "theme"  "Children" }
EOF

like($response, qr/HTTP\/1\.1 400 Bad Request/, 'Got a 400.');
like($response, qr/Content-Type: application\/json/i, 'Content-type is application/json');
like($response, qr/Expected : between key:value pair./i, "Got the missing colon message");

# 9. Api key missing
$response = ApiManager::http_get($NginxPort,'/shelves/1');

like($response, qr/HTTP\/1\.1 401 Unauthorized/, 'Got a 401.');
like($response, qr/Content-Type: application\/json/i, 'Content-type is application/json');
like($response, qr/Method doesn't allow unregistered callers/i,
    'Got the "unregistered caller" message');

################################################################################

sub service_control {
    my ($t, $port, $file) = @_;

    my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
        or die "Can't create test server socket: $!\n";

    local $SIG{PIPE} = 'IGNORE';

    # Failed check response
    my $failed_check = ServiceControl::convert_proto(<<'EOF', 'check_response', 'binary');
{
  "operationId": "ListShelves:7b3f4c4f-f29c-4391-b35e-0a676427fec8",
  "checkErrors": [
    {
      "code": "SERVICE_NOT_ACTIVATED",
      "detail": "Project is not activated."
    }
  ]
}
EOF

    my $first_check = 1;

    $server->on_sub('POST', '/v1/services/endpoints-transcoding-test.cloudendpointsapis.com:check', sub {
            my ($headers, $body, $client) = @_;

            # Fail the first check to simulate service-control error and succeed the rest
            if ($first_check) {
                print $client <<EOF . $failed_check;
HTTP/1.1 200 OK
Connection: close

EOF
                $first_check = 0;
            } else {
                print $client <<EOF;
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
            }
        });

    $server->on_sub('POST', '/v1/services/endpoints-transcoding-test.cloudendpointsapis.com:report', sub {
            my ($headers, $body, $client) = @_;
            print $client <<EOF;
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
        });


    $server->run();
}

################################################################################