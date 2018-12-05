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
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $CloudTracePort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(29);

my $config = ApiManager::get_bookstore_service_config_allow_some_unregistered .
    ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF";
producer_project_id: "api-manager-project"
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF
$t->write_file('service.pb.txt', $config);

$t->write_file('server_config.pb.txt', <<"EOF");
cloud_tracing_config {
  url_override: "http://127.0.0.1:${CloudTracePort}"
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
my $trace_done = 'trace_done';

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);
$t->run_daemon(\&cloudtrace, $t, $CloudTracePort, 'cloudtrace.log', $trace_done);

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${CloudTracePort}"), 1, 'Cloud trace socket ready.');

$t->run();

################################################################################

# This request triggers trace.
my $trace_id = 'e133eacd437d8a12068fd902af3962d8';
my $parent_span_id = '12345678';
my $response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
X-Cloud-Trace-Context: ${trace_id}/${parent_span_id};o=1

EOF

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
is($t->waitforfile("$t->{_testdir}/${trace_done}"), 1, 'Trace body file ready.');
$t->stop_daemons();

my @trace_requests = ApiManager::read_http_stream($t, 'cloudtrace.log');


# Verify there is only one trace request.
is(scalar @trace_requests, 1, 'Cloud Trace received 1 request.');

my $trace_request = shift @trace_requests;
is($trace_request->{verb}, 'PATCH', 'Cloud Trace: request is PATCH.');
is($trace_request->{uri}, '/v1/projects/api-manager-project/traces',
    'Trace request was called with correct project id in url.');

my $json_obj = decode_json($trace_request->{body});
is($json_obj->{traces}->[0]->{projectId}, 'api-manager-project', 'Project ID in body is correct.');
is($json_obj->{traces}->[0]->{traceId}, $trace_id, 'Trace ID matches the provided one.');
is($json_obj->{traces}->[0]->{spans}->[0]->{name},
    'endpoints-test.cloudendpointsapis.com/ListShelves',
    'Root trace span name is set to method name of ListShelves');
is($json_obj->{traces}->[0]->{spans}->[0]->{kind}, 'RPC_SERVER', 'Trace span kind is RPC_SERVER');
is($json_obj->{traces}->[0]->{spans}->[0]->{parentSpanId}, $parent_span_id,
    'Parent span of root should be the provided one');
my $agent = $json_obj->{traces}->[0]->{spans}->[0]->{labels}->{'trace.cloud.google.com/agent'};
is($agent, 'esp/' . ServiceControl::get_version(), 'Agent is set to "esp/xxx".');
my $rootid = $json_obj->{traces}->[0]->{spans}->[0]->{spanId};
is($json_obj->{traces}->[0]->{spans}->[1]->{name}, 'CheckServiceControl',
    'Next trace span is CheckServiceControl');
is($json_obj->{traces}->[0]->{spans}->[1]->{parentSpanId}, $rootid,
    'Parent of CheckServiceControlCache span is root');
my $check_service_control_id = $json_obj->{traces}->[0]->{spans}->[1]->{spanId};
is($json_obj->{traces}->[0]->{spans}->[2]->{name}, 'CheckServiceControlCache',
    'Next trace span is CheckServiceControlCache');
is($json_obj->{traces}->[0]->{spans}->[2]->{parentSpanId}, $check_service_control_id,
    'Parent of CheckServiceControlCache span is CheckServiceControl');
my $check_service_control_cache_id = $json_obj->{traces}->[0]->{spans}->[2]->{spanId};
is($json_obj->{traces}->[0]->{spans}->[3]->{name}, 'Call ServiceControl server',
    'Next trace span is Call ServiceControl server');
is($json_obj->{traces}->[0]->{spans}->[3]->{parentSpanId}, $check_service_control_cache_id,
    'Parent of Call ServiceControl sever span is CheckServiceControlCache');
is($json_obj->{traces}->[0]->{spans}->[4]->{name}, 'QuotaControl',
    'Next trace span is Backend');
is($json_obj->{traces}->[0]->{spans}->[4]->{parentSpanId}, $rootid,
    'Parent of Beckend span is root');
is($json_obj->{traces}->[0]->{spans}->[5]->{name}, 'Backend',
    'Next trace span is Backend');
is($json_obj->{traces}->[0]->{spans}->[5]->{parentSpanId}, $rootid,
    'Parent of Beckend span is root');
my $backend_span_id = $json_obj->{traces}->[0]->{spans}->[5]->{spanId};

my @bookstore_requests = ApiManager::read_http_stream($t, 'bookstore.log');
is(scalar @bookstore_requests, 1, 'Bookstore received 1 request.');

my $bookstore_request = shift @bookstore_requests;
is($bookstore_request->{verb}, 'GET', 'backend received a get');
is($bookstore_request->{path}, '/shelves', 'backend received get /shelves');
my $backend_trace_header = $bookstore_request->{headers}->{'x-cloud-trace-context'};
isnt($backend_trace_header, undef, 'X-Cloud-Trace-Context was received');
is($backend_trace_header, $trace_id . '/' . $backend_span_id . ';o=1',
    'X-Cloud-Trace-Context header is correct.');


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

Shelves data.
EOF
  });

  $server->run();
}

################################################################################

sub cloudtrace {
  my ($t, $port, $file, $done) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
      or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('PATCH', '/v1/projects/api-manager-project/traces', sub {
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
