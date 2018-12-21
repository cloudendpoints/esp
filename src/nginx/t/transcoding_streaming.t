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
my $ServiceControlPort = ApiManager::pick_port();
my $GrpcServerPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(18);

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
      grpc_pass 127.0.0.1:${GrpcServerPort} override;
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

my $bulk_shelves_response = ApiManager::http($NginxPort,<<EOF);
POST /bulk/shelves?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 158

[
{ "theme" : "Classics" },
{ "theme" : "Satire" },
{ "theme" : "Russian" },
{ "theme" : "Children" },
{ "theme" : "Documentary" },
{ "theme" : "Mystery" },
]
EOF

my $final_shelves_response = ApiManager::http_get($NginxPort,'/shelves?key=api-key');

# Wait for the service control report
is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
$t->stop_daemons();

# Check the requests that the backend has received
my $initial_shelves_request_expected = {};
my $bulk_shelves_request1_expected = { 'shelf' => {'theme' => 'Classics'}};
my $bulk_shelves_request2_expected = { 'shelf' => {'theme' => 'Satire'}};
my $bulk_shelves_request3_expected = { 'shelf' => {'theme' => 'Russian'}};
my $bulk_shelves_request4_expected = { 'shelf' => {'theme' => 'Children'}};
my $bulk_shelves_request5_expected = { 'shelf' => {'theme' => 'Documentary'}};
my $bulk_shelves_request6_expected = { 'shelf' => {'theme' => 'Mystery'}};
my $final_shelves_request_expected = {};

my $server_output = $t->read_file('server.log');
my @server_requests = split /\r\n\r\n/, $server_output;

ok(ApiManager::compare_json($server_requests[0], $initial_shelves_request_expected));
ok(ApiManager::compare_json($server_requests[1], $bulk_shelves_request1_expected));
ok(ApiManager::compare_json($server_requests[2], $bulk_shelves_request2_expected));
ok(ApiManager::compare_json($server_requests[3], $bulk_shelves_request3_expected));
ok(ApiManager::compare_json($server_requests[4], $bulk_shelves_request4_expected));
ok(ApiManager::compare_json($server_requests[5], $bulk_shelves_request5_expected));
ok(ApiManager::compare_json($server_requests[6], $bulk_shelves_request6_expected));
ok(ApiManager::compare_json($server_requests[7], $final_shelves_request_expected));

# Check responses
my $initial_shelves_response_expected = {
  'shelves' => [
    {'id' => '1', 'theme' => 'Fiction'},
    {'id' => '2', 'theme' => 'Fantasy'},
  ]
};
my $bulk_shelves_response_expected = [
   {'id' => '3', 'theme' => 'Classics'},
   {'id' => '4', 'theme' => 'Satire'},
   {'id' => '5', 'theme' => 'Russian'},
   {'id' => '6', 'theme' => 'Children'},
   {'id' => '7', 'theme' => 'Documentary'},
   {'id' => '8', 'theme' => 'Mystery'},
];

my $final_shelves_response_expected = {
  'shelves' => [
    {'id' => '1', 'theme' => 'Fiction'},
    {'id' => '2', 'theme' => 'Fantasy'},
    {'id' => '3', 'theme' => 'Classics'},
    {'id' => '4', 'theme' => 'Satire'},
    {'id' => '5', 'theme' => 'Russian'},
    {'id' => '6', 'theme' => 'Children'},
    {'id' => '7', 'theme' => 'Documentary'},
    {'id' => '8', 'theme' => 'Mystery'},
  ]
};

ok(ApiManager::verify_http_json_response($initial_shelves_response, $initial_shelves_response_expected),
                                         "Initial shelves response is good");
ok(ApiManager::verify_http_json_response($bulk_shelves_response, $bulk_shelves_response_expected),
                                         "Bulk create shelves response is good");
ok(ApiManager::verify_http_json_response($final_shelves_response, $final_shelves_response_expected),
                                         "Final shelves response is good");

# Check service control calls
# We expect 3 service control calls:
#   - 1 check call for both calls to ListShelves (as we are using the same API key the
#     second time check response is in the cache)
#   - 1 check call for the call to BulkCreateShelf,
#   - 1 aggregated report call containing 2 operations - ListShelves and BulkCreateShelf
my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 3, 'Service control was called 3 times');

my $report_request = pop @servicecontrol_requests;
like($report_request->{uri}, qr/:report$/, 'Report has correct uri');
my $report_json = decode_json(ServiceControl::convert_proto($report_request->{body}, "report_request", "json"));
my @operations = @{$report_json->{operations}};
is(scalar @operations, 2, 'There are 2 operations');

################################################################################

sub service_control {
  my ($t, $port, $file, $done) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";

  local $SIG{PIPE} = 'IGNORE';

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
    $t->write_file($done, ":report done");
  });

  $server->run();
}

################################################################################
