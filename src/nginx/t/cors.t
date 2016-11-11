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

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(15);

# Save service name in the service configuration protocol buffer file.
my $config = ApiManager::get_bookstore_service_config .
             ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF";
producer_project_id: "esp-project-id"
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF
$t->write_file('service.pb.txt', $config);

# Disable service control caching to guarantee consistent sequence of requests
# to service control server
$t->write_file('server_config.pb.txt', ApiManager::disable_service_control_cache);

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
        api service.pb.txt;
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
my $response1 = ApiManager::http($NginxPort,<<'EOF');
OPTIONS /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
Access-Control-Request-Method: GET
Access-Control-Request-Headers: authorization
Origin: http://google.com/bookstore/root
Referer: http://google.com/bookstore/root

EOF

my $response2 = ApiManager::http($NginxPort,<<'EOF');
OPTIONS /shelves/1 HTTP/1.0
Host: localhost
Access-Control-Request-Method: GET
Access-Control-Request-Headers: authorization
Origin: http://google.com/bookstore/root
Referer: http://google.com/bookstore/root

EOF

is($t->waitforfile("$t->{_testdir}/${report_done}-1"), 1, 'First report is done.');
is($t->waitforfile("$t->{_testdir}/${report_done}-2"), 1, 'Second report is done.');
$t->stop_daemons();

my ($response_headers1, $response_body1) = split /\r\n\r\n/, $response1, 2;
my ($response_headers2, $response_body2) = split /\r\n\r\n/, $response2, 2;

like($response_headers1, qr/HTTP\/1\.1 204 OK/, 'Returned 204 in the response1');
is($response_body1, <<'EOF', 'Empty body returned in the response body1.');
EOF
like($response_headers2, qr/HTTP\/1\.1 204 OK/, 'Returned 204 in the resposne2.');
is($response_body2, <<'EOF', 'Empty body returned in the response body2.');
EOF

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 3, 'Service control was called 3 times');

# :check 1
my $r = shift @servicecontrol_requests;
like($r->{uri}, qr/:check$/, 'First call was a :check');

my $check_body1 = ServiceControl::convert_proto($r->{body}, 'check_request', 'json');
my $expected_check_body1 = {
  'serviceName' => 'endpoints-test.cloudendpointsapis.com',
  'serviceConfigId' => '2016-08-25r1',
  'operation' => {
     'consumerId' => 'api_key:this-is-an-api-key',
     'operationName' => 'CorsShelves',
     'labels' => {
        'servicecontrol.googleapis.com/caller_ip' => '127.0.0.1',
        'servicecontrol.googleapis.com/service_agent' => ServiceControl::service_agent(),
        'servicecontrol.googleapis.com/user_agent' => 'ESP',
        'servicecontrol.googleapis.com/referer' => 'http://google.com/bookstore/root',
     }
  }
};
ok(ServiceControl::compare_json($check_body1, $expected_check_body1), 'Check body 1 is received.');

# :report 1
$r = shift @servicecontrol_requests;
like($r->{uri}, qr/:report$/, 'Second call was a :report');

my $report_body1 = ServiceControl::convert_proto($r->{body}, 'report_request', 'json');
my $expected_report_body1 = ServiceControl::gen_report_body({
  'serviceConfigId' => '2016-08-25r1',
  'serviceName' =>  'endpoints-test.cloudendpointsapis.com',
  'url' => '/shelves?key=this-is-an-api-key',
  'api_key' => 'this-is-an-api-key',
  'producer_project_id' => 'esp-project-id',
  'referer' => 'http://google.com/bookstore/root',
  'location' => 'us-central1',
  'api_method' =>  'CorsShelves',
  'http_method' => 'OPTIONS',
  'log_message' => 'Method: CorsShelves',
  'response_code' => '204',
  'request_size' => 230,
  'response_size' => 229,
  });
ok(ServiceControl::compare_json($report_body1, $expected_report_body1), 'Report body 1 is received.');

# :report 2
$r = shift @servicecontrol_requests;
like($r->{uri}, qr/:report$/, 'Last call was a :report');

my $report_body2 = ServiceControl::convert_proto($r->{body}, 'report_request', 'json');
my $expected_report_body2 = ServiceControl::gen_report_body({
  'serviceConfigId' => '2016-08-25r1',
  'serviceName' =>  'endpoints-test.cloudendpointsapis.com',
  'url' => '/shelves/1',
  'producer_project_id' => 'esp-project-id',
  'no_consumer_data' => 1,
  'referer' => 'http://google.com/bookstore/root',
  'location' => 'us-central1',
  'api_method' =>  'CORS',
  'http_method' => 'OPTIONS',
  'log_message' => 'Method: CORS',
  'response_code' => '204',
  'request_size' => 209,
  'response_size' => 229,
  });
ok(ServiceControl::compare_json($report_body2, $expected_report_body2), 'Report body 2 is received.');

################################################################################

sub servicecontrol {
  my ($t, $port, $file, $done) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';
  my $report_counter = 1;

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
    $t->write_file("${done}-${report_counter}", ':report done');
    ++$report_counter;
  });

  $server->run();
}

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('OPTIONS', '/shelves', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 204 OK
Access-Control-Allow-Headers:authorization
Access-Control-Allow-Methods:GET,HEAD,PUT,PATCH,POST,DELETE
Access-Control-Allow-Origin:*
Connection: close

EOF

  });

  $server->on_sub('OPTIONS', '/shelves/1', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 204 OK
Access-Control-Allow-Headers:authorization
Access-Control-Allow-Methods:GET,HEAD,PUT,PATCH,POST,DELETE
Access-Control-Allow-Origin:*
Connection: close

EOF

  });

  $server->run();
}

################################################################################
