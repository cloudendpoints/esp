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

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(18);

# Save service name in the service configuration protocol buffer file.
my $config = ApiManager::get_bookstore_service_config_allow_unregistered .
             ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF";
producer_project_id: "endpoints-test"
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF
$t->write_file('service.pb.txt', $config);

# Disable service control caching to guarantee consistent sequence of requests
# to service control server
$t->write_file('service_control.pb.txt', <<"EOF");
service_control_config {
  check_aggregator_config {
    cache_entries: 0
  }
  report_aggregator_config {
    cache_entries: 0
  }
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
  server_tokens off;
  server {
    listen 127.0.0.1:${NginxPort};
    server_name localhost;
    location / {
      endpoints {
        api service.pb.txt;
        server_config service_control.pb.txt;
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

my $response1 = ApiManager::http_get($NginxPort,'/shelves');
my $response2 = ApiManager::http_get($NginxPort,'/shelves/0');

is($t->waitforfile("$t->{_testdir}/${report_done}-1"), 1, 'First report is done.');
is($t->waitforfile("$t->{_testdir}/${report_done}-2"), 1, 'Second report is done.');

$t->stop_daemons();

my ($response1_headers, $response1_body) = split /\r\n\r\n/, $response1, 2;
my ($response2_headers, $response2_body) = split /\r\n\r\n/, $response2, 2;

like($response1_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
like($response1_body, qr/Shelves data/, 'Received response data');

like($response2_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200 after failed report.');
like($response2_body, qr/Shelves 0 data/, 'Received response data after failed report');

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 4, 'Service control was called four times.');

my $r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':check was a POST');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check',
   ':check was called');

$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':report was a POST');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:report',
   ':report was called');

$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':check was a POST');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check',
   ':check was called');

$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':report was a POST');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:report',
   ':report was called');

# Verify an error message for failed report
my $error_log = $t->read_file('error.log');
like($error_log, qr/.*[error].*Service control report failed.*/, 'Got the warnings from nginx.');

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
HTTP/1.1 500 Internal Server Error
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

  $server->on('GET', '/shelves', <<'EOF');
HTTP/1.1 200 OK
Connection: close

Shelves data
EOF

  $server->on('GET', '/shelves/0', <<'EOF');
HTTP/1.1 200 OK
Connection: close

Shelves 0 data
EOF

  $server->run();
}

################################################################################
