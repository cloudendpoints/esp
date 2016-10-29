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
use JSON::PP;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework

################################################################################

# Port assignments
my $ServiceControlPort = ApiManager::pick_port();
my $Http2NginxPort = ApiManager::pick_port();
my $GrpcBackendPort = ApiManager::pick_port();
my $GrpcFallbackPort = ApiManager::pick_port();
my $CloudTracePort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(15);
$t->write_file('service.pb.txt',
        ApiManager::get_grpc_test_service_config($GrpcBackendPort) .
        ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

# Set the trace cache to 0 so that trace request will be sent immediately.
$t->write_file('server_config.pb.txt', <<"EOF");
cloud_tracing_config {
  url_override: "http://127.0.0.1:${CloudTracePort}"
  aggregation_config {
    time_millisec: 100000
    cache_max_size: 0
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
  server {
    listen 127.0.0.1:${Http2NginxPort} http2;
    server_name localhost;
    location / {
      endpoints {
        api service.pb.txt;
        server_config server_config.pb.txt;
        on;
      }
      grpc_pass 127.0.0.2:${GrpcFallbackPort};
    }
  }
}
EOF

my $report_done = 'report_done';
my $trace_done = 'trace_done';

$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);
$t->run_daemon(\&cloudtrace, $t, $CloudTracePort, 'cloudtrace.log', $trace_done);
$t->run_daemon(\&ApiManager::grpc_test_server, $t, "127.0.0.1:${GrpcBackendPort}");
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${CloudTracePort}"), 1, 'Cloud trace socket ready.');
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
    }
    request {
      text: "Hello, world!"
    }
  }
}
EOF

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
is($t->waitforfile("$t->{_testdir}/${trace_done}"), 1, 'Trace body file ready.');
$t->stop_daemons();

my $test_results_expected = <<'EOF';
results {
  echo {
    text: "Hello, world!"
  }
}
EOF
is($test_results, $test_results_expected, 'Client tests completed as expected.');

my @trace_requests = ApiManager::read_http_stream($t, 'cloudtrace.log');
is(scalar @trace_requests, 1, 'Cloud Trace received 1 request');
my $trace_request = shift @trace_requests;
is($trace_request->{verb}, 'PATCH', 'Cloud Trace: request is PATCH.');
is($trace_request->{uri}, '/v1/projects/endpoints-grpc-test/traces',
    'Trace request was called with correct project id in url.');

my $json_obj = decode_json($trace_request->{body});
is($json_obj->{traces}->[0]->{projectId}, 'endpoints-grpc-test', 'Project ID in body is correct.');
is($json_obj->{traces}->[0]->{spans}->[0]->{name},
    'endpoints-grpc-test.cloudendpointsapis.com.test.grpc.Test.Echo',
    'Root trace span name is set to method name');
is($json_obj->{traces}->[0]->{spans}->[0]->{kind}, 'RPC_SERVER', 'Trace span kind is RPC_SERVER');
is($json_obj->{traces}->[0]->{spans}->[0]->{parentSpanId}, undef,
    'Parent span id is not set');
my $agent = $json_obj->{traces}->[0]->{spans}->[0]->{labels}->{'trace.cloud.google.com/agent'};
is($agent, 'esp/' . ServiceControl::get_version(), 'Agent is set to "esp/xxx".');

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

sub cloudtrace {
  my ($t, $port, $file, $done) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
      or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('PATCH', '/v1/projects/endpoints-grpc-test/traces', sub {
        my ($headers, $body, $client) = @_;
        print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

{}
EOF
        $t->write_file($done, ':trace done');
      });

  $server->run();
}

################################################################################