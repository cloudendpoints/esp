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
use JSON::PP;
use ServiceControl;

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $MetadataPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(55);

# Save service name in the service configuration protocol buffer file.
my $config = ApiManager::get_echo_service_config .
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
  endpoints {
    metadata_server http://127.0.0.1:${MetadataPort};
  }
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

sub get_request {
  my ($method) = @_;

  if ($method eq 'GET' || $method eq 'DELETE') {
    return <<"EOF";
$method /echo?key=this-is-an-api-key HTTP/1.0
Host: localhost

EOF

  }
  return <<"EOF";
$method /echo?key=this-is-an-api-key HTTP/1.0
content-type: application/json
Host: localhost

{}
EOF
}

foreach my $method ('GET', 'POST', 'PATCH', 'PUT', 'DELETE') {
  my $report_done = "report_done-${method}";
  my $bookstore_log = "bookstore-${method}.log";
  my $servicecontrol_log = "servicecontrol-${method}.log";
  my $metadata_log = "metadata-${method}.log";

  $t->run_daemon(\&bookstore, $t, $BackendPort, $bookstore_log, $method);
  $t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, $servicecontrol_log, $report_done);
  $t->run_daemon(\&metadata, $t, $MetadataPort, $metadata_log);

  is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
  is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
  is($t->waitforsocket("127.0.0.1:${MetadataPort}"), 1, 'Metadata server socket ready.');

  $t->run();

  my $response = ApiManager::http($NginxPort,get_request($method));

  is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
  $t->stop();
  $t->stop_daemons();

  my @servicecontrol_requests = ApiManager::read_http_stream($t, $servicecontrol_log);
  is(scalar @servicecontrol_requests, 2, 'Service control was called twice.');

  # :check
  my $r = shift @servicecontrol_requests;
  like($r->{uri}, qr/:check$/, ':check was called');

  # :report
  $r = shift @servicecontrol_requests;
  like($r->{uri}, qr/:report$/, ':check was called');

  my $report_json = decode_json(ServiceControl::convert_proto($r->{body}, 'report_request', 'json'));
  my $operations = $report_json->{operations};
  is(1, @{$operations}, "There are 1 operations total for $method");
  my ($report) = @$operations;

  my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;
  like($response_headers, qr/HTTP\/1\.1 200 OK/, "$method method returned HTTP 200.");
  is($response_body, <<"EOF", "$method method response body is verified.");
{ "method": "$method" }
EOF

  is($report->{logEntries}[0]->{structPayload}->{http_method}, $method,
     "$method method in Report log is verified.");
}

################################################################################

sub servicecontrol {
  my ($t, $port, $file, $done) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";

  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('POST', '/v1/services/echo.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/echo.cloudendpointsapis.com:report', sub {
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

sub metadata {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/computeMetadata/v1/?recursive=true', <<'EOF' . ApiManager::get_metadata_response_body);
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

EOF

  $server->on('GET', '/computeMetadata/v1/instance/service-accounts/default/token', <<'EOF');
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

{
 "access_token":"ya29.7gFRTEGmovWacYDnQIpC9X9Qp8cH0sgQyWVrZaB1Eg1WoAhQMSG4L2rtaHk1",
 "expires_in":100,
 "token_type":"Bearer"
}
EOF

  $server->run();
}

################################################################################

sub bookstore {
  my ($t, $port, $file, $method) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on($method, '/echo', <<"EOF");
HTTP/1.1 200 OK
Connection: close

{ "method": "$method" }
EOF

  $server->run();
}

################################################################################
