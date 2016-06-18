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

BEGIN { use FindBin; chdir($FindBin::Bin); }

use ApiManager;   # Must be first (sets up import path to the Nginx test module)
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework
use HttpServer;
use ServiceControl;

################################################################################

# Port assignments
my $GrpcPort = 8080;
my $ServiceControlPort = 8081;
my $GrpcServerPort = 8082;

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(11);

$t->write_file('service.pb.txt',
               ApiManager::get_grpc_test_service_config .
               ApiManager::read_test_file('testdata/logs_metrics.pb.txt') .
               <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
system_parameters {
  rules {
    selector: "test.grpc.Test.Echo"
    parameters {
      name: "api_key"
      http_header: "x-api-key"
    }
  }
}
EOF

ApiManager::write_file_expand($t, 'nginx.conf', <<"EOF");
%%TEST_GLOBALS%%
daemon off;
events {
  worker_connections 32;
}
grpc {
  listen 127.0.0.1:${GrpcPort};
  api service.pb.txt;
}
http {
  %%TEST_GLOBALS_HTTP%%
  server_tokens off;
  server {
    listen 127.0.0.1:8083;
    server_name localhost;
    location / {
      endpoints {
        api service.pb.txt;
        %%TEST_CONFIG%%
        on;
      }
      proxy_pass http://127.0.0.1:8085;
    }
  }
}
EOF

my $report_done = 'report_done';

$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'requests.log', $report_done);
$t->run_daemon(\&ApiManager::grpc_test_server, $t, "127.0.0.1:${GrpcServerPort}");
$t->run_daemon(\&ApiManager::not_found_server, $t, 8085);
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${GrpcServerPort}"), 1, 'GRPC test server socket ready.');
$t->run();
is($t->waitforsocket("127.0.0.1:${GrpcPort}"), 1, 'Grpc socket ready.');

################################################################################

my $test_results = &ApiManager::run_grpc_test($t, <<"EOF");
server_addr: "127.0.0.1:${GrpcPort}"
plans {
  echo {
    request {
      text: "Hello, world!"
    }
  }
}
EOF

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
$t->stop_daemons();

my $test_results_expected = <<'EOF';
results {
  status {
    code: 16
    details: "Method doesn\'t allow unregistered callers (callers without established identity). Please use API Key or other form of API consumer identity to call this API."
  }
}
EOF
is($test_results, $test_results_expected,
   'Client tests completed as expected.');

my @requests = ApiManager::read_http_stream($t, 'requests.log');
is(scalar @requests, 1, 'Service control received one call.');

my $r = shift @requests;
is($r->{verb}, 'POST', ':report was called via POST');
is($r->{uri}, '/v1/services/endpoints-grpc-test.cloudendpointsapis.com:report', ':report was called');
is($r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}", ':report call has Host header');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', 'Content-Type is application/x-protobuf');

my $report_body = ServiceControl::convert_proto($r->{body}, 'report_request', 'json');
my $expected_report_body = ServiceControl::gen_report_body({
  'producer_project_id' => 'endpoints-grpc-test',
  'location' => 'us-central1',
  'api_name' =>  'endpoints-grpc-test.cloudendpointsapis.com',
  'api_method' =>  'test.grpc.Test.Echo',
  'http_method' => 'POST',
  'log_message' => 'Failed to call method: test.grpc.Test.Echo',
  'protocol' => 'grpc',
  'response_code' => '401',
  'error_cause' => 'service_control',
  'error_type' => '4xx',
  'request_size' => 0,
  'response_size' => 157,
  'status_code' => '16',
});

ok(ServiceControl::compare_json($report_body, $expected_report_body), 'Report body is received.');

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
