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

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(14);

$t->write_file('sc_override.pb.txt', <<"EOF");
service_control_config {
  log_request_header: "foo"
  log_request_header: "bar"
  log_response_header: "foo2"
  log_response_header: "bar2"
}
EOF

# Save service name in the service configuration protocol buffer file.
my $config = ApiManager::get_bookstore_service_config_allow_some_unregistered .
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
        server_config sc_override.pb.txt;
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

# Issues two identical requests
my $response1 = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
bar: bar_value

EOF

my $response2 = ApiManager::http($NginxPort,<<"EOF");
GET /shelves/Fiction/books?key=this-is-an-api-key HTTP/1.0
Host: localhost
bar: bar_value
foo: foo_value
un_configured_header: invalid_value

EOF

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
$t->stop_daemons();

my ($response_headers1, $response_body1) = split /\r\n\r\n/, $response1, 2;

like($response_headers1, qr/HTTP\/1\.1 200 OK/, 'response1 returned HTTP 200.');
is($response_body1, <<'EOF', 'Shelves returned in the response1 body.');
Shelves data.
EOF

# Should have only one Check request, second one should use the cached one.
my @requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @requests, 2, 'Service control was called twice.');

my $r = shift @requests;
is($r->{verb}, 'POST', ':check was called via POST');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', ':check was called');

$r = shift @requests;
is($r->{verb}, 'POST', ':report was a POST');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:report',
   ':report was called');

my $report_json = decode_json(ServiceControl::convert_proto($r->{body}, 'report_request', 'json'));

my $log = $report_json->{operations}[0]->{logEntries}[0]->{structPayload};
is($log->{request_headers}, 'bar=bar_value;', 'log message includes request headers');
is($log->{response_headers}, 'foo2=response_foo;bar2=response_bar;', 'log message includes response headers');

$log = $report_json->{operations}[1]->{logEntries}[0]->{structPayload};
is($log->{request_headers}, 'foo=foo_value;bar=bar_value;', 'log message includes request headers');
is($log->{response_headers}, 'foo2=response_foo2;', 'log message includes response headers');

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
foo2: response_foo
bar2: response_bar

Shelves data.
EOF
  });

  $server->on('GET', '/shelves/Fiction/books?key=this-is-an-api-key', <<'EOF');
HTTP/1.1 200 OK
Content-Type: application/text
Connection: close
foo2: response_foo2
unconfigured_header: unconfigured

Never Let Me Go
1Q84
EOF

  $server->run();
}

################################################################################