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
my $ServiceControlPort = ApiManager::pick_port();
my $Http2NginxPort = ApiManager::pick_port();
my $GrpcBackendPort = ApiManager::pick_port();
my $GrpcFallbackPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(9);
$t->write_file('service.pb.txt',
        ApiManager::get_grpc_test_service_config($GrpcBackendPort) .
        ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
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
  server {
    listen 127.0.0.1:${Http2NginxPort} http2;
    server_name localhost;
    location / {
      endpoints {
        api service.pb.txt;
        %%TEST_CONFIG%%
        on;
      }
      grpc_pass 127.0.0.2:${GrpcFallbackPort};
    }
  }
}
EOF

my $report_done = 'report_done';

$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);
$t->run_daemon(\&ApiManager::grpc_test_server, $t, "127.0.0.1:${GrpcBackendPort}");
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${GrpcBackendPort}"), 1, 'GRPC test server socket ready.');
$t->run();
is($t->waitforsocket("127.0.0.1:${Http2NginxPort}"), 1, 'Nginx socket ready.');

################################################################################
my $test_results = &ApiManager::run_grpc_test($t, <<"EOF");
server_addr: "127.0.0.1:${Http2NginxPort}"
plans {
  echo {
    call_config {
      api_key: "this-is-an-api-key"
      compression: GZIP
    }
    request {
      text: "This text must be long enough for GRPC library compress it. ______________________________________________________________"
    }
  }
}
EOF

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
$t->stop_daemons();

my $test_results_expected = <<'EOF';
results {
  echo {
    text: "This text must be long enough for GRPC library compress it. ______________________________________________________________"
  }
}
EOF
is($test_results, $test_results_expected, 'Client tests completed as expected.');

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 2, 'Service control was called twice');

# :check
my $r = shift @servicecontrol_requests;
like($r->{uri}, qr/:check$/, 'First call was a :check');

# :report
$r = shift @servicecontrol_requests;
like($r->{uri}, qr/:report$/, 'Second call was a :report');

my $report_body = ServiceControl::convert_proto($r->{body}, 'report_request', 'json');
my $expected_report_body = ServiceControl::gen_report_body({
  'serviceConfigId' => '2016-08-25r1',
  'serviceName' =>  'endpoints-grpc-test.cloudendpointsapis.com',
  'api_method' => 'test.grpc.Test.Echo',
  'url' => '/test.grpc.Test/Echo',
  'protocol' => 'grpc',
  'api_key' => 'this-is-an-api-key',
  'api_name' => 'test.grpc.Test',
  'producer_project_id' => 'endpoints-grpc-test',
  'http_method' => 'POST',
  'log_message' => 'Method: test.grpc.Test.Echo',
  'response_code' => '200',
  'request_size' => ($^O eq 'darwin' ? 405 : 407),
  'request_bytes' => ($^O eq 'darwin' ? 405 : 407),
  'streaming_request_message_counts' => 1,
  'streaming_response_message_counts' => 1,
  });
ok(ServiceControl::compare_http2_report_json($report_body, $expected_report_body), 'Report body is received.');

################################################################################

sub service_control {
  my ($t, $port, $file, $done) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";

  $server->on_sub('POST', '/v1/services/endpoints-grpc-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/endpoints-grpc-test.cloudendpointsapis.com:report', sub {
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
