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
my $GrpcNginxPort = 8080;
my $ServiceControlPort = 8081;
my $GrpcBackendPort = 8082;
my $HttpNginxPort = 8083;
my $HttpBackendPort = 8085;

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(16);

$t->write_file('service.pb.txt',
               ApiManager::get_grpc_test_service_config .
               ApiManager::read_test_file('testdata/logs_metrics.pb.txt') .
               <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
system_parameters {
  rules {
    selector: "test.grpc.Test.EchoStream"
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
  listen 127.0.0.1:${GrpcNginxPort};
  api service.pb.txt;
}
http {
  %%TEST_GLOBALS_HTTP%%
  server_tokens off;
  server {
    listen 127.0.0.1:${HttpNginxPort};
    server_name localhost;
    location / {
      endpoints {
        api service.pb.txt;
        %%TEST_CONFIG%%
        on;
      }
      proxy_pass http://127.0.0.1:${HttpBackendPort};
    }
  }
}
EOF

my $report_done = 'report_done';

$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'requests.log', $report_done);
$t->run_daemon(\&ApiManager::grpc_test_server, $t, "127.0.0.1:${GrpcBackendPort}");
$t->run_daemon(\&ApiManager::not_found_server, $t, $HttpBackendPort);
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${GrpcBackendPort}"), 1, 'GRPC test server socket ready.');
$t->run();
is($t->waitforsocket("127.0.0.1:${GrpcNginxPort}"), 1, 'Nginx GRPC socket ready.');

################################################################################
my $test_results = &ApiManager::run_grpc_test($t, <<"EOF");
server_addr: "127.0.0.1:${GrpcNginxPort}"
plans {
  echo_stream {
    request {
      text: "Hello, world!"
    }
    call_config {
      api_key: "this-is-an-api-key"
    }
    count: 100
  }
}
EOF

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
$t->stop_daemons();

my $test_results_expected = <<'EOF';
results {
  echo_stream {
    count: 100
  }
}
EOF
is($test_results, $test_results_expected, 'Client tests completed as expected.');

my @requests = ApiManager::read_http_stream($t, 'requests.log');
is(scalar @requests, 2, 'Service control received two requests.');

# :check
my $r = shift @requests;
is($r->{verb}, 'POST', ':check was a post');
is($r->{uri}, '/v1/services/endpoints-grpc-test.cloudendpointsapis.com:check', ':check was called');
is($r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}", 'Host header was set on :check.');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', ':check Content-Type is protocol buffer.');

my $check_body = ServiceControl::convert_proto($r->{body}, 'check_request', 'json');
my $expected_check_body = {
  'serviceName' => 'endpoints-grpc-test.cloudendpointsapis.com',
  'operation' => {
     'consumerId' => 'api_key:this-is-an-api-key',
     'operationName' => 'test.grpc.Test.EchoStream',
     'labels' => {
        'servicecontrol.googleapis.com/caller_ip' => '127.0.0.1',
        'servicecontrol.googleapis.com/user_agent' => 'ESP',
     }
  }
};
ok(ServiceControl::compare_json($check_body, $expected_check_body), 'Check body is received.');

# :report
$r = shift @requests;
is($r->{verb}, 'POST', ':report was a post');
is($r->{uri}, '/v1/services/endpoints-grpc-test.cloudendpointsapis.com:report', ':report was called');
is($r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}", 'Host header was set on :report.');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', ':report Content-Type is protocol buffer.');

my $report_body = ServiceControl::convert_proto($r->{body}, 'report_request', 'json');
my $expected_report_body = ServiceControl::gen_report_body({
  'api_key' => 'this-is-an-api-key',
  'location' => 'us-central1',
  'api_name' =>  'endpoints-grpc-test.cloudendpointsapis.com',
  'api_method' =>  'test.grpc.Test.EchoStream',
  'http_method' => 'POST',
  'log_message' => 'Method: test.grpc.Test.EchoStream',
  'protocol' => 'grpc',
  'response_code' => '200',
  'request_size' => 1500,
  'response_size' => 2600,
  'producer_project_id' => 'endpoints-grpc-test',
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
