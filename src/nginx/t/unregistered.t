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
use src::nginx::t::ServiceControl;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework
use JSON::PP;

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $MetadataPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(29);

# Use JSON configuration.
ApiManager::write_file_expand($t, 'service.json', <<"EOF");
{
 "name": "endpoints-test.cloudendpointsapis.com",
 "http": {
  "rules": [
   {
    "selector": "ListShelves",
    "get": "/shelves"
   },
   {
    "selector": "GetShelf",
    "get": "/shelves/{shelf}"
   },
   {
    "selector": "ListBooks",
    "get": "/shelves/{shelf}/books"
   },
   {
    "selector": "GetBook",
    "get": "/shelves/{shelf}/books/{book}"
   }
  ]
 },
 "usage": {
  "rules": [
   {
    "selector": "ListShelves",
    "allowUnregisteredCalls": true
   },
   {
    "selector": "GetShelf",
    "allowUnregisteredCalls": true
   },
   {
    "selector": "ListBooks",
    "allowUnregisteredCalls": false
   },
   {
    "selector": "GetBook",
    "allowUnregisteredCalls": false
   }
  ]
 },
 "control": {
  "environment": "http://127.0.0.1:${ServiceControlPort}"
 }
}
EOF

ApiManager::write_file_expand($t, 'nginx.conf', <<"EOF");
%%TEST_GLOBALS%%
daemon off;
events {
  worker_connections 32;
}
http {
  %%TEST_GLOBALS_HTTP%%
  server_tokens off;
  endpoints {
    metadata_server http://127.0.0.1:${MetadataPort};
  }
  server {
    listen 127.0.0.1:${NginxPort};
    server_name localhost;
    location / {
      endpoints {
        api service.json;
        %%TEST_CONFIG%%
        on;
      }
      proxy_pass http://127.0.0.1:${BackendPort};
    }
  }
}
EOF

my $report_done = 'report_done';

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);
$t->run_daemon(\&metadata, $t, $MetadataPort, 'metadata.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${MetadataPort}"), 1, 'Metadata server socket ready.');

$t->run();

################################################################################

# API key, allows unregistered.
my $response1 = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key-1');
# No API Key, allows unregistered.
my $response2 = ApiManager::http_get($NginxPort,'/shelves/2');
# API key, doesn't allow unregistered.
my $response3 = ApiManager::http_get($NginxPort,'/shelves/3/books?key=this-is-an-api-key-3');
# No API Key, doesn't allow unregistered.
my $response4 = ApiManager::http_get($NginxPort,'/shelves/4/books/4');

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');

$t->stop();
$t->stop_daemons();

################################################################################

#  my $json_body = ;

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 4, 'Service control was called four times.');

my $report_request = pop @servicecontrol_requests;
like($report_request->{uri}, qr/^.*:report$/, 'Report was called.');
my $report_json = decode_json(ServiceControl::convert_proto($report_request->{body}, 'report_request', 'json'));
my @operations = @{$report_json->{operations}};
is(scalar @operations, 4, 'There are 4 operations total');

# /shelves?key=this-is-an-api-key-1
like($response1, qr/HTTP\/1\.1 200 OK/, 'Allow unregistered, provide API key - 200 OK');
like($response1, qr/List of shelves\.$/, 'Allow unregistered, provide API key - body');

my $r = shift @servicecontrol_requests;
like($r->{uri}, qr/:check$/, 'Check was called for /shelves?key=this-is-an-api-key.');
my $check = decode_json(ServiceControl::convert_proto($r->{body}, 'check_request', 'json'));
is($check->{operation}->{operationName}, 'ListShelves',
   'Allow unregistered, provide API key - method name');
is($check->{operation}->{consumerId}, 'api_key:this-is-an-api-key-1',
   'Allow unregistered, provide API key - consumer id is correct');
my $report = shift @operations;
is($report->{consumerId}, 'api_key:this-is-an-api-key-1',
   'Allow unregistered, provide API key - report_body has correct consumerId');

# /shelves/2
like($response2, qr/HTTP\/1\.1 200 OK/, 'Allow unregistered, no API key - 200 OK');
like($response2, qr/Shelf 2\.$/, 'Allow unregistered, no API key - body');

$r = shift @servicecontrol_requests;
like($r->{uri}, qr/:check$/, 'Check was called for /shelves/2');
$check = decode_json(ServiceControl::convert_proto($r->{body}, 'check_request', 'json'));
is($check->{operation}->{operationName}, 'GetShelf',
   'Allow unregistered, no API key - method name');
is($check->{operation}->{consumerId}, 'project:esp-test-app',
   'Allow unregistered, no API key - consumer id is correct');
$report = shift @operations;
is($report->{consumerId}, 'project:esp-test-app',
   'Allow unregistered, no API key - report_body has correct consumerId');

# /shelves/3/books?key=this-is-an-api-key-3
like($response3, qr/HTTP\/1\.1 200 OK/, 'Disallow unregistered, provide API key - 200 OK');
like($response3, qr/List of books on shelf 3\.$/, 'Disallow unregistered, provide API key - body');

$r = shift @servicecontrol_requests;
like($r->{uri}, qr/:check$/, 'Disallow unregistered, provide API key - :check');
$check = decode_json(ServiceControl::convert_proto($r->{body}, 'check_request', 'json'));
is($check->{operation}->{operationName}, 'ListBooks',
   'Disallow unregistered, provide API key - method name');
is($check->{operation}->{consumerId}, 'api_key:this-is-an-api-key-3',
   'Disallow unregistered, provide API key - consumer id is correct');
$report = shift @operations;
is($report->{consumerId}, 'api_key:this-is-an-api-key-3',
   'Disallow unregistered, provide API key - report_body has correct consumerId');

# /shelves/4/books/4
like($response4, qr/HTTP\/1\.1 401 Unauthorized/, 'Disallow unregistered, no API key - 401 Unauthorized');
like($response4, qr/"message": "Method doesn't allow unregistered callers/,
     'Disallow unregistered, no API key - error body');

is(scalar @servicecontrol_requests, 0,
   'Disallow unregistered, no API key - check service control not called.');
$report = shift @operations;
is($report->{consumerId}, 'project:esp-test-app',
   'Disallow unregistered, no API key - report_body has correct consumerId');

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/shelves?key=this-is-an-api-key-1', <<'EOF');
HTTP/1.1 200 OK
Connection: close

List of shelves.
EOF

  $server->on('GET', '/shelves/2', <<'EOF');
HTTP/1.1 200 OK
Connection: close

Shelf 2.
EOF

  $server->on('GET', '/shelves/3/books?key=this-is-an-api-key-3', <<'EOF');
HTTP/1.1 200 OK
Connection: close

List of books on shelf 3.
EOF

  $server->run();
}

sub servicecontrol {
  my ($t, $port, $file, $done) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:report', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
    $t->write_file($done, ':report done');
  });

  $server->run();
}

sub metadata {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/computeMetadata/v1/?recursive=true', <<'EOF');
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

{
  "instance": {
    "attributes": {
      "gae_backend_name": "default",
      "gae_backend_version": "20150921t180445"
    },
    "hostname": "gae-default-20150921t180445-inqp.c.esp-test-app.internal",
    "zone": "projects/345623948572/zones/us-west1-d"
  },
  "project": {
    "projectId": "esp-test-app"
  }
}
EOF

  $server->on('GET', '/computeMetadata/v1/instance/service-accounts/default/token', <<'EOF');
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

{
 "access_token":"ya29.7gFRTEGmovWacYDnQIpC9X9Qp8cH0sgQyWVrZaB1Eg1WoAhQMSG4L2rtaHk1",
 "expires_in":200,
 "token_type":"Bearer"
}
EOF

  $server->run();
}

################################################################################
