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
use JSON::PP;
use Data::Dumper;

################################################################################

# Port assignments
my $NginxPort = 8080;
my $BackendPort = 8081;
my $ServiceControlPort = 8082;

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(41);

# Save service name in the service configuration protocol buffer file.
my $config = ApiManager::get_bookstore_service_config .
             ApiManager::read_test_file('testdata/system_parameters.pb.txt') .
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

my $report_done_file = 'report_done';

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log', $report_done_file);
$t->run();
is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');

################################################################################

my $response1 = http_get('/shelves?key=key-1');
my $response2 = http_get('/shelves?api_key=api-key-1');
my $response3 = http_get('/shelves?api_key=api-key-2&key=key-2');

my $response4 = http(<<'EOF');
GET /shelves HTTP/1.0
KEY: key-4
Host: localhost

EOF

my $response5 = http(<<'EOF');
GET /shelves HTTP/1.0
API-KEY: key-5
Host: localhost

EOF

my $response6 = http(<<'EOF');
GET /shelves HTTP/1.0
KEY: key-61
API-KEY: key-62
Host: localhost

EOF

my $response7 = http(<<'EOF');
GET /shelves?api_key=api-key-72&key=key-71 HTTP/1.0
KEY: key-73
API-KEY: key-74
Host: localhost

EOF

is($t->waitforfile("$t->{_testdir}/${report_done_file}"), 1, 'Report body file ready.');
$t->stop_daemons();

like($response1, qr/HTTP\/1\.1 200 OK/, 'Response1 for key-1 - 200 OK');
like($response1, qr/List of shelves\.$/, 'Response1 for key-1 - body');
like($response2, qr/HTTP\/1\.1 200 OK/, 'Response2 for api-key-1 - 200 OK');
like($response2, qr/List of shelves\.$/, 'Response2 for api-key-1 - body');
like($response3, qr/HTTP\/1\.1 200 OK/, 'Response3 for api-key-2&key-2 - 200 OK');
like($response3, qr/List of shelves\.$/, 'Response3 for api-key-2&key-2 - body');
like($response4, qr/HTTP\/1\.1 200 OK/, 'Response4 KEY: key-4 - 200 OK');
like($response4, qr/List of shelves\.$/, 'Response KEY: key-4 - body');
like($response5, qr/HTTP\/1\.1 200 OK/, 'Response5 API-KEY: key-5 - 200 OK');
like($response5, qr/List of shelves\.$/, 'Response5 API-KEY: key-5 - body');
like($response6, qr/HTTP\/1\.1 200 OK/, 'Response6 KEY:&API-KEY: - 200 OK');
like($response6, qr/List of shelves\.$/, 'Response6 KEY:&API-KEY: - body');
like($response7, qr/HTTP\/1\.1 200 OK/, 'Response7 - 200 OK');
like($response7, qr/List of shelves\.$/, 'Response7 - body');

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 8, 'Service control was called 8 times');

my $report_request = pop @servicecontrol_requests;
like($report_request->{uri}, qr/:report$/, 'Report has correct uri');
my $report_json = decode_json(ServiceControl::convert_proto($report_request->{body}, 'report_request', 'json'));
my @operations = @{$report_json->{operations}};
is(scalar @operations, 7, 'There are 7 report operations total');

sub verify_request {
  my ($r, $report_op, $consumer_id, $comment) = @_;

  like($r->{uri}, qr/:check$/, ":check ${comment} has correct uri");
  my $check = decode_json(ServiceControl::convert_proto($r->{body}, 'check_request', 'json'));
  is($check->{operation}->{consumerId}, $consumer_id,
     ":check ${comment} body has correct consumer id (${consumer_id}).");
  is($report_op->{consumerId}, ${consumer_id},
     ":report operation ${comment} has correct consumerId (${consumer_id})");
}

my @consumer_ids = (
  'api_key:key-1',
  'api_key:api-key-1',
  'api_key:key-2',
  'api_key:key-4',
  'api_key:key-5',
  'api_key:key-61',
  'api_key:key-71'
);

# Verify all consumer IDs were included correctly in :check and :report calls.
foreach my $ci (0 .. $#consumer_ids) {
  verify_request($servicecontrol_requests[$ci], $operations[$ci], $consumer_ids[$ci], $ci + 1);
}

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

List of shelves.
EOF
  });

  $server->run();
}

################################################################################
