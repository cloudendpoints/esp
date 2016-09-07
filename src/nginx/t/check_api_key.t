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
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(26);

# Save service name in the service configuration protocol buffer file.
my $config = ApiManager::get_bookstore_service_config .
             ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF";
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

my $report_done = $t->{_testdir} . '/report_done.log';

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, $report_done,
               'servicecontrol.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');

$t->run();

################################################################################

my $response1 = ApiManager::http_get($NginxPort,'/shelves?key=key-1');
my $response2 = ApiManager::http_get($NginxPort,'/shelves?api_key=api-key-1');
my $response3 = ApiManager::http_get($NginxPort,'/shelves?api_key=api-key-2&key=key-2');
my $response4 = ApiManager::http($NginxPort,<<'EOF');
GET /shelves HTTP/1.0
X-API-KEY: key-4
Host: localhost

EOF

is($t->waitforfile($report_done), 1, 'Report body file ready.');
$t->stop_daemons();

like($response1, qr/HTTP\/1\.1 200 OK/, 'Response for key-1 - 200 OK');
like($response1, qr/List of shelves\.$/, 'Response for key-1 - body');
like($response2, qr/HTTP\/1\.1 200 OK/, 'Response for api-key-1 - 200 OK');
like($response2, qr/List of shelves\.$/, 'Response for api-key-1 - body');
like($response3, qr/HTTP\/1\.1 200 OK/, 'Response for api-key-2&key-2 - 200 OK');
like($response3, qr/List of shelves\.$/, 'Response for api-key-2&key-2 - body');
like($response4, qr/HTTP\/1\.1 200 OK/, 'Response for x-api-key:key-4 - 200 OK');
like($response4, qr/List of shelves\.$/, 'Response for x-api-key:key-4 - body');

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 5, 'Service control was called 5 times.');

my ($check1, $check2, $check3, $check4, $report) = @servicecontrol_requests;
like($check1->{uri}, qr/:check$/, 'Check 1 uri');
like($check2->{uri}, qr/:check$/, 'Check 2 uri');
like($check3->{uri}, qr/:check$/, 'Check 3 uri');
like($check4->{uri}, qr/:check$/, 'Check 4 uri');
like($report->{uri}, qr/:report$/, 'Report uri');

# Parse out operations from the report.
my $report_json = decode_json(ServiceControl::convert_proto($report->{body}, 'report_request', 'json'));
my $operations = $report_json->{operations};
is(@{$operations}, 4, 'There are 4 report operations total');
my ($report1, $report2, $report3, $report4) = @{$operations};

# Match the report operations to checks.
my $check1_body = decode_json(ServiceControl::convert_proto($check1->{body}, 'check_request', 'json'));
is($check1_body->{operation}->{consumerId}, 'api_key:key-1',
   'check body has correct consumer id for key-1.');
is($report1->{consumerId}, 'api_key:key-1',
   'report_body has correct consumerId for key-1');

my $check2_body = decode_json(ServiceControl::convert_proto($check2->{body}, 'check_request', 'json'));
is($check2_body->{operation}->{consumerId}, 'api_key:api-key-1',
   'check body has correct consumer id for api-key-1.');
is($report2->{consumerId}, 'api_key:api-key-1',
   'report_body has correct consumerId for api-key-1');

my $check3_body = decode_json(ServiceControl::convert_proto($check3->{body}, 'check_request', 'json'));
is($check3_body->{operation}->{consumerId}, 'api_key:key-2',
   'check body has correct consumer id for api-key-2&key-2.');
is($report3->{consumerId}, 'api_key:key-2',
   'report_body has correct consumerId for api-key-2&key-2');

my $check4_body = decode_json(ServiceControl::convert_proto($check4->{body}, 'check_request', 'json'));
is($check4_body->{operation}->{consumerId}, 'api_key:key-4',
   'check body has correct consumer id for key-4.');
is($report4->{consumerId}, 'api_key:key-4',
   'report_body has correct consumerId for key-4');

################################################################################

sub servicecontrol {
  my ($t, $port, $done, $file) = @_;
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
    ApiManager::write_binary_file($done, ':report done');
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

List of shelves.
EOF
  });

  $server->run();
}

################################################################################
