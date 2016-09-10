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
my $ServiceControlPort = ApiManager::pick_port();
my $GrpcServerPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(21);

$t->write_file('service.json',
  ApiManager::get_transcoding_test_service_config(
    'endpoints-transcoding-test.cloudendpointsapis.com',
    "http://127.0.0.1:${ServiceControlPort}"));

$t->write_file('server_config.pb.txt', ApiManager::disable_service_control_cache);

$t->write_file_expand('nginx.conf', <<EOF);
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
      grpc_pass 127.0.0.1:${GrpcServerPort};
    }
  }
}
EOF

my $report_done = 'report_done';

$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);
ApiManager::run_transcoding_test_server($t, 'server.log', "127.0.0.1:${GrpcServerPort}");

is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, "Service control socket ready.");
is($t->waitforsocket("127.0.0.1:${GrpcServerPort}"), 1, "GRPC test server socket ready.");
$t->run();
is($t->waitforsocket("127.0.0.1:${NginxPort}"), 1, "Nginx socket ready.");

################################################################################

my $initial_shelves_response = ApiManager::http_get($NginxPort,'/shelves?key=api-key');

my $shelf1_response = ApiManager::http($NginxPort,<<EOF);
POST /shelves?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 24

{ "theme" : "Classics" }
EOF

my $shelf2_response = ApiManager::http($NginxPort,<<EOF);
POST /shelves?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Transfer-Encoding: chunked

9
{ "theme"
7
 : "Chi
8
ldren" }
0

EOF

my $final_shelves_response = ApiManager::http_get($NginxPort,'/shelves?key=api-key');

# Wait for the service control report
is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
$t->stop_daemons();

# Check the requests that the backend has received
my $initial_shelves_request_expected = {};
my $shelf1_request_expected = { 'shelf' => {'theme' => 'Classics'}};
my $shelf2_request_expected = { 'shelf' => {'theme' => 'Children'}};
my $final_shelves_request_expected = {};

my $server_output = $t->read_file('server.log');
my @server_requests = split /\r\n\r\n/, $server_output;

ok(ApiManager::compare_json($server_requests[0], $initial_shelves_request_expected));
ok(ApiManager::compare_json($server_requests[1], $shelf1_request_expected));
ok(ApiManager::compare_json($server_requests[2], $shelf2_request_expected));
ok(ApiManager::compare_json($server_requests[3], $final_shelves_request_expected));

# Check responses
my $initial_shelves_response_expected = {
  'shelves' => [
    {'id' => '1', 'theme' => 'Fiction'},
    {'id' => '2', 'theme' => 'Fantasy'},
  ]
};
my $shelf1_response_expected = {'id' => '3', 'theme' => 'Classics'};
my $shelf2_response_expected = {'id' => '4', 'theme' => 'Children'};
my $final_shelves_response_expected = {
  'shelves' => [
    {'id' => '1', 'theme' => 'Fiction'},
    {'id' => '2', 'theme' => 'Fantasy'},
    {'id' => '3', 'theme' => 'Classics'},
    {'id' => '4', 'theme' => 'Children'},
  ]
};

ok(ApiManager::verify_http_json_response($initial_shelves_response, $initial_shelves_response_expected),
                                         "Initial shelves response is good");
ok(ApiManager::verify_http_json_response($shelf1_response, $shelf1_response_expected),
                                         "Add shelf1 response is good");
ok(ApiManager::verify_http_json_response($shelf2_response, $shelf2_response_expected),
                                         "Add shelf2 response is good");
ok(ApiManager::verify_http_json_response($final_shelves_response, $final_shelves_response_expected),
                                         "Final shelves response is good");

# Expect 8 service control calls for 4 requests.
my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 8, 'Service control was called 8 times');

while (my ($i, $r) = each @servicecontrol_requests) {
  if ($i % 2 == 0) {
    like($r->{uri}, qr/:check$/, "Check has correct uri for ${i}");
  } else {
    like($r->{uri}, qr/:report$/, "Report has correct uri for ${i}");
  }
}

################################################################################

sub service_control {
  my ($t, $port, $file, $done) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";

  local $SIG{PIPE} = 'IGNORE';
  my $request_count = 0;

  $server->on_sub('POST', '/v1/services/endpoints-transcoding-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<EOF;
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/endpoints-transcoding-test.cloudendpointsapis.com:report', sub {
    my ($headers, $body, $client) = @_;
    print $client <<EOF;
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
    $request_count++;
    if ($request_count == 4) {
      $t->write_file($done, ":report done");
    }
  });

  $server->run();
}

################################################################################
