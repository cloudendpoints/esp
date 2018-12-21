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
use src::nginx::t::Auth;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $PubkeyPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(9);

# Save service name in the service configuration protocol buffer file.
my $config = ApiManager::get_bookstore_service_config_allow_unregistered .
             ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF";
producer_project_id: "endpoints-test"
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
authentication {
  providers {
    id: "test_auth"
    issuer: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l\@developer.gserviceaccount.com"
    jwks_uri: "http://127.0.0.1:${PubkeyPort}/pubkey"
  }
  rules {
    selector: "ListShelves"
    requirements {
      provider_id: "test_auth"
      audiences: "ok_audience_1,ok_audience_2"
    }
  }
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

my $report_done = 'report_done';

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);
$t->run_daemon(\&pubkey, $t, $PubkeyPort, Auth::get_public_key_jwk, 'pubkey.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');

$t->run();

################################################################################

my $token = Auth::get_auth_token('./src/nginx/t/matching-client-secret.json');
my $response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves HTTP/1.0
Authorization: Bearer $token
Host: localhost

EOF

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
$t->stop_daemons();

my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body, <<'EOF', 'Shelves returned in the response body.');
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
# Check was not called since no api_key and allow_unregistered_calls.
is(scalar @servicecontrol_requests, 1, 'Service control was called once');

# :report
my $r = shift @servicecontrol_requests;
like($r->{uri}, qr/:report$/, 'The call was a :report');

my $report_body = ServiceControl::convert_proto($r->{body}, 'report_request', 'json');
my $expected_report_body = ServiceControl::gen_report_body({
  'serviceConfigId' => '2016-08-25r1',
  'serviceName' =>  'endpoints-test.cloudendpointsapis.com',
  'url' => '/shelves',
  'producer_project_id' => 'endpoints-test',
  'no_consumer_data' => 1,
  'api_method' =>  'ListShelves',
  'http_method' => 'GET',
  'log_message' => 'Method: ListShelves',
  'response_code' => '200',
  'request_size' => 844,
  'response_size' => 208,
  'request_bytes' => 844,
  'response_bytes' => 208,
  'jwtAuth' => 'issuer=NjI4NjQ1NzQxODgxLW5vYWJpdTIzZjVhOG04b3ZkOHVjdjY5OGxqNzh2djBsQGRldmVsb3Blci5nc2VydmljZWFjY291bnQuY29t&audience=ZW5kcG9pbnRzLXRlc3QuY2xvdWRlbmRwb2ludHNhcGlzLmNvbQ',
  });

ok(ServiceControl::compare_json($report_body, $expected_report_body), 'Report body is received.');

################################################################################

sub servicecontrol {
  my ($t, $port, $file, $done) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:report', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
    $t->write_file($done, ':report done');
  });

  $server->run();
}

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('GET', '/shelves', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF
  });

  $server->run();
}

################################################################################

sub pubkey {
  my ($t, $port, $pkey, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/pubkey', <<"EOF");
HTTP/1.1 200 OK
Connection: close

$pkey
EOF
  $server->run();
}
