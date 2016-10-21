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

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(9);

# Save service name in the service configuration protocol buffer file.
my $config = ApiManager::get_bookstore_service_config .
    ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF";
producer_project_id: "endpoints-test"
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

my $report_done = 'report_done';

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');

$t->run();

################################################################################

my $response = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key');

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
$t->stop_daemons();

my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

like($response, qr/HTTP\/1\.1 400 Bad Request/, 'Response returned HTTP 400.');

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 2, 'Service control was called twice.');

# :check
my $r = shift @servicecontrol_requests;
like($r->{uri}, qr/:check$/, ':check was called');

my $check_body = ServiceControl::convert_proto($r->{body}, 'check_request', 'json');
my $expected_check_body = {
  'serviceName' => 'endpoints-test.cloudendpointsapis.com',
  'serviceConfigId' => '2016-08-25r1',
  'operation' => {
     'consumerId' => 'api_key:this-is-an-api-key',
     'operationName' => 'ListShelves',
     'labels' => {
        'servicecontrol.googleapis.com/caller_ip' => '127.0.0.1',
        'servicecontrol.googleapis.com/service_agent' => ServiceControl::service_agent(),
        'servicecontrol.googleapis.com/user_agent' => 'ESP',
     }
  }
};
ok(ServiceControl::compare_json($check_body, $expected_check_body), 'Check body is received.');

# :report
$r = shift @servicecontrol_requests;
like($r->{uri}, qr/:report$/, ':report was called');

my $report_body = ServiceControl::convert_proto($r->{body}, 'report_request', 'json');
print $report_body;
my $expected_report_body = ServiceControl::gen_report_body({
        'serviceConfigId' => '2016-08-25r1',
        'serviceName' =>  'endpoints-test.cloudendpointsapis.com',
        'url' => '/shelves?key=this-is-an-api-key',
        'location' => 'us-central1',
        'producer_project_id' => 'endpoints-test',
        'api_method' =>  'ListShelves',
        'http_method' => 'GET',
        'log_message' => 'Method: ListShelves',
        'response_code' => '400',
        'error_cause' => 'service_control',
        'error_type' => '4xx',
        'request_size' => 62,
        'response_size' => 345,
        'request_bytes' => 62,
        'response_bytes' => 345,
        'no_consumer_data' => 1,
    });

ok(ServiceControl::compare_json($report_body, $expected_report_body), 'Report body is received.');

################################################################################

sub servicecontrol {
    my ($t, $port, $file, $done) = @_;
    my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
        or die "Can't create test server socket: $!\n";
    local $SIG{PIPE} = 'IGNORE';

    # This is normal 200 HTTP response with error in CheckResponse Json
    my $proto_response = ServiceControl::convert_proto(<<'EOF', 'check_response', 'binary');
{
  "operationId": "ListShelves:7b3f4c4f-f29c-4391-b35e-0a676427fec8",
  "checkErrors": [
    {
      "code": "API_KEY_INVALID",
      "detail": "Invalid api key."
    }
  ]
}
EOF
    $server->on('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', <<'EOF' . $proto_response);
HTTP/1.1 200 OK
Connection: close

EOF

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

    $server->run();
}

################################################################################
