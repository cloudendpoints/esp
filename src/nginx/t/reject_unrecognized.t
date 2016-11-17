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
use src::nginx::t::Auth;
use src::nginx::t::ServiceControl;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework
use JSON::PP;

################################################################################

# Port assigments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $MetadataPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(16);

$t->write_file('service.pb.txt',
               ApiManager::get_bookstore_service_config() .
               ApiManager::read_test_file('testdata/logs_metrics.pb.txt') .
               <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

ApiManager::write_file_expand($t, 'nginx.conf', <<"EOF");
%%TEST_GLOBALS%%
daemon off;
events { worker_connections 32; }
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

my $report_done = 'report_done';

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);
$t->run_daemon(\&metadata, $t, $MetadataPort, 'metadata.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${MetadataPort}"), 1, 'Metadata socket ready.');

$t->run();

################################################################################

# Call an unrecognized method "merge shelves"
my $response = ApiManager::http_get($NginxPort,'/shelves/1:merge?other=2');

# Wait for :report body.
is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');

$t->stop_daemons();

# Verify the response body.
my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;
like($response_headers, qr/HTTP\/1\.1 404 Not Found/,
     'API call returned HTTP 404.');
like($response_body, qr/"Method does not exist."/, 'Response container an error.');

# Verify service control :check was not called and :report was.
my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 1, 'Service control was called once.');

# :report
my $r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':report was a POST');
like($r->{uri}, qr/:report$/, ':report was called');

# Verify the :report body contents.
my $report_json = decode_json(ServiceControl::convert_proto($r->{body}, 'report_request', 'json'));

ok((not exists($report_json->{operations}[0]->{consumerId})), 'ConsumerID not set');
is($report_json->{operations}[0]->{operationName}, '<Unknown Operation Name>',
   'Method was not recognized.');

my $log = $report_json->{operations}[0]->{logEntries}[0];
is($log->{name}, 'endpoints_log', 'Log entry was written into endpoints_log');
my $payload = $log->{structPayload};
is($payload->{http_method}, 'GET', 'Logged HTTP verb is GET');
is($payload->{url}, '/shelves/1:merge?other=2', 'Logged URL is correct');
like($payload->{log_message}, qr/an unrecognized HTTP call/, 'Logged message is as expected');

# Verify backend was not called.
is($t->read_file('bookstore.log'), '', 'Backend was not called.');

################################################################################

sub servicecontrol {
  my ($t, $port, $file, $done) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  # Both :check and :report will be called.
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

  # Backend is not expected to receive a call.

  $server->run();
}

################################################################################
# Metadata server.

sub metadata {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create metadata server socket: $!\n";
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
 "expires_in":200,
 "token_type":"Bearer"
}
EOF

  $server->run();
}
