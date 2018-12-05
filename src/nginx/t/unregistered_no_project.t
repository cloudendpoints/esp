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
use JSON::PP;

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(31);

$t->write_file('server_config.pb.txt', ApiManager::disable_service_control_cache);

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

$t->write_file_expand('nginx.conf', <<"EOF");
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
        api service.json;
        server_config server_config.pb.txt;
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

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');

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

sub verify_check {
  my ($r, $call, $operation_name, $consumer_id) = @_;

  like($r->{uri}, qr/:check$/, 'Check was called for ' . $call);
  my $check = decode_json(ServiceControl::convert_proto($r->{body}, 'check_request', 'json'));
  is($check->{operation}->{operationName}, $operation_name,
     'Check matched operation name for ' . $call);
  is($check->{operation}->{consumerId}, $consumer_id,
     'Check matched consumer id for ' . $call);
}

sub verify_report {
  my ($r, $call, $operation_name, $consumer_id) = @_;

  like($r->{uri}, qr/:report$/, 'Report was called for ' . $call);
  my $report_json = decode_json(ServiceControl::convert_proto($r->{body}, 'report_request', 'json'));
  my @operations = @{$report_json->{operations}};
  is(scalar @operations, 1, 'There are 1 operations total');
  my $report = shift @operations;
  is($report->{operationName}, $operation_name,
     'Matched operation name for ' . $call);
  if (defined $consumer_id) {
    is($report->{consumerId}, $consumer_id, 'Report matched consumer id for ' . $call);
  } else {
    ok((not exists($report->{consumerId})), 'Report has empty consumerId for ' . $call);
  }
}

# 1st call: Check is NOT called since api-key is not required even though it is provided
# 2nd call: Check is NOT called since api-key is not required nor provided
# 3rd call: Check is called since api-key is provided
# 4th call: Check is NOT called since api-key is NOT provided
# Report are called for all of them.
my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 5, 'Service control was called 5 times.');

my ($report1, $report2, $check3, $report3, $report4) = @servicecontrol_requests;

# /shelves?key=this-is-an-api-key-1
verify_report($report1,
            '/shelves?key=this-is-an-api-key',
            'ListShelves',
            undef);

like($response1, qr/HTTP\/1\.1 200 OK/, 'Allow unregistered, provide API key - 200 OK');
like($response1, qr/List of shelves\.$/, 'Allow unregistered, provide API key - body');

# /shelves/2
verify_report($report2,
              '/shelves/2',
              'GetShelf',
              undef);

like($response2, qr/HTTP\/1\.1 200 OK/, 'Allow unregistered, no API key - 200 OK');
like($response2, qr/List of shelves 2\.$/, 'Allow unregistered, no API key - body');

# /shelves/3/books?key=this-is-an-api-key-3
verify_check($check3,
            '/shelves/3/books?key=this-is-an-api-key-3',
            'ListBooks',
            'api_key:this-is-an-api-key-3');

verify_report($report3,
            '/shelves/3/books?key=this-is-an-api-key-3',
            'ListBooks',
            'api_key:this-is-an-api-key-3');


like($response3, qr/HTTP\/1\.1 200 OK/, 'Disallow unregistered, provide API key - 200 OK');
like($response3, qr/List of books on shelf 3\.$/, 'Disallow unregistered, provide API key - body');

# /shelves/4/books/4
verify_report($report4,
              '/shelves/4/books/4',
              'GetBook',
               undef);

like($response4, qr/HTTP\/1\.1 401 Unauthorized/, 'Disallow unregistered, no API key - 401 Unauthorized');
like($response4, qr/"message": "Method doesn't allow unregistered callers/,
     'Disallow unregistered, no API key - error body');

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

List of shelves 2.
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
  my $request_count = 0;

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
    $request_count++;
    if ($request_count == 4) {
        $t->write_file($done, ':report done');
    }
  });

  $server->run();
}

################################################################################
