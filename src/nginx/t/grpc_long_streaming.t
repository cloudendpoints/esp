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
my @streaming_bytes_metrics = (
    'serviceruntime.googleapis.com/api/producer/request_bytes',
    'serviceruntime.googleapis.com/api/consumer/request_bytes',
    'serviceruntime.googleapis.com/api/consumer/response_bytes',
    'serviceruntime.googleapis.com/api/producer/response_bytes',
);

my @final_metrics = (
    'serviceruntime.googleapis.com/api/consumer/total_latencies',
    'serviceruntime.googleapis.com/api/producer/total_latencies',
    'serviceruntime.googleapis.com/api/consumer/backend_latencies',
    'serviceruntime.googleapis.com/api/producer/backend_latencies',
    'serviceruntime.googleapis.com/api/consumer/request_overhead_latencies',
    'serviceruntime.googleapis.com/api/producer/request_overhead_latencies',
    'serviceruntime.googleapis.com/api/consumer/streaming_durations',
    'serviceruntime.googleapis.com/api/producer/streaming_durations',
    "serviceruntime.googleapis.com/api/producer/streaming_request_message_counts",
    "serviceruntime.googleapis.com/api/consumer/streaming_request_message_counts",
    "serviceruntime.googleapis.com/api/producer/streaming_response_message_counts",
    "serviceruntime.googleapis.com/api/consumer/streaming_response_message_counts",
);

################################################################################

# Port assignments
my $Http2NginxPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $GrpcBackendPort = ApiManager::pick_port();
my $GrpcFallbackPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(78);

$t->write_file('service.pb.txt',
    ApiManager::get_grpc_test_service_config($GrpcBackendPort) .
        ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

$t->write_file('server_config.pb.txt', <<"EOF");
service_control_config {
  report_aggregator_config {
    cache_entries: 0
  }
}
streaming_report_config {
  intermediate_time_window: 1
  threshold_in_bytes: 50
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
        server_config server_config.pb.txt;
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
  echo_stream {
    call_config {
      api_key: "this-is-an-api-key"
    }
    request {
      text: "Hello, world!"
    }
    duration_in_sec: 2
  }
}
EOF

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
$t->stop_daemons();

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 3, 'Service control was called 3 times.');

# :check
my $r = shift @servicecontrol_requests;
like($r->{uri}, qr/:check$/, 'First call was a :check');

# :intermediate report
$r = shift @servicecontrol_requests;
like($r->{uri}, qr/:report$/, 'Second call was a :report');

my $report = decode_json(ServiceControl::convert_proto($r->{body}, 'report_request', 'json'));

sub find_in_array {
    my ($name, $value, $array_ref) = @_;
    foreach my $op (@{$array_ref}) {
        if (exists $op->{$name} && $op->{$name} eq $value) {
            return $op;
        }
    }
    return undef;
}

my $echo_stream = find_in_array('operationName', 'test.grpc.Test.EchoStream', $report->{operations});
isnt($echo_stream, undef, 'Found EchoStream operation');

# Declare variales used below.
my ($mn, $metrics, $metric, $labels);

$labels = $echo_stream->{labels};
isnt($labels, undef, 'GetShelf operation has labels');
is($labels->{'/credential_id'}, 'apikey:this-is-an-api-key',
    'GetShelf credential id is an API key.');
is($labels->{'/protocol'}, 'grpc', 'EchoStream protocol is grpc');

$metrics = $echo_stream->{metricValueSets};
isnt($metrics, undef, 'GetShelf has metrics');

# request_count is 1 in the first report
my $mn = 'serviceruntime.googleapis.com/api/producer/request_count';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "First report has $mn metric.");
is($metric->{metricValues}[0]->{int64Value}, '1', "First report $mn is 1.");

$mn = 'serviceruntime.googleapis.com/api/consumer/request_count';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "First report has $mn metric.");
is($metric->{metricValues}[0]->{int64Value}, '1', "First report $mn is 1.");

# request_bytes/response_bytes are greater than 0 in the first report
foreach $mn (@streaming_bytes_metrics)
{
    $metric = find_in_array('metricName', $mn, $metrics);
    isnt($metric, undef, "First report has $mn metric.");
    cmp_ok($metric, 'gt',  0, "The metric value is great than 0.");
}

# There are no other metrics in the first report
foreach $mn (@final_metrics)
{
    $metric = find_in_array('metricName', $mn, $metrics);
    is($metric, undef, "First report does not have $mn metric.");
}

# :final report
$r = shift @servicecontrol_requests;
like($r->{uri}, qr/:report$/, 'Third call was a :report');

$report = decode_json(ServiceControl::convert_proto($r->{body}, 'report_request', 'json'));
my $echo_stream = find_in_array('operationName', 'test.grpc.Test.EchoStream', $report->{operations});
isnt($echo_stream, undef, 'Found EchoStream operation');

$labels = $echo_stream->{labels};
isnt($labels, undef, 'GetShelf operation has labels');
is($labels->{'/credential_id'}, 'apikey:this-is-an-api-key',
    'GetShelf credential id is an API key.');
is($labels->{'/protocol'}, 'grpc', 'EchoStream protocol is grpc');
is($labels->{'/response_code'}, '200', 'EchoStream response code is 200');
is($labels->{'/response_code_class'}, '2xx', 'EchoStream response code class is 2xx');

$metrics = $echo_stream->{metricValueSets};
isnt($metrics, undef, 'GetShelf has metrics');

# request_count is not existed in the final report as it is already sent in the first report.
my $mn = 'serviceruntime.googleapis.com/api/producer/request_count';
$metric = find_in_array('metricName', $mn, $metrics);
is($metric, undef, "Final report does not have $mn metric.");

$mn = 'serviceruntime.googleapis.com/api/consumer/request_count';
$metric = find_in_array('metricName', $mn, $metrics);
is($metric, undef, "Final report does not have $mn metric.");

# request_bytes/response_bytes are greater than 0 in the final report
foreach $mn (@streaming_bytes_metrics)
{
    $metric = find_in_array('metricName', $mn, $metrics);
    isnt($metric, undef, "Final report has $mn metric.");
    cmp_ok($metric, 'gt',  0, "The metric value is great than 0.");
}

# Other metrics are existed in the final report
foreach $mn (@final_metrics)
{
    $metric = find_in_array('metricName', $mn, $metrics);
    isnt($metric, undef, "Final report has $mn metric.");
    is($metric->{metricValues}[0]->{distributionValue}->{count}, '1',
        "Metric $mn has 1 value.");
}
################################################################################

sub service_control {
    my ($t, $port, $file, $done) = @_;
    my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
        or die "Can't create test server socket: $!\n";
    my $report_count = 0;

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
            $report_count++;
            if($report_count == 2) {
                $t->write_file($done, ':report done');
            }
        });

    $server->run();
}

################################################################################
