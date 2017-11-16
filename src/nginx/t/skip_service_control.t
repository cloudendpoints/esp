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
my $MetadataPort = ApiManager::pick_port();
my $CloudTracePort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(8);

# Use JSON configuration.
ApiManager::write_file_expand($t, 'service.json', <<"EOF");
{
 "name": "endpoints-test.cloudendpointsapis.com",
 "http": {
  "rules": [
   {
    "selector": "ListShelves",
    "get": "/shelves"
   }
  ]
 },
 "usage": {
  "rules": [
   {
    "selector": "ListShelves",
    "skipServiceControl": true
   }
  ]
 },
 "control": {
  "environment": "http://127.0.0.1:${ServiceControlPort}"
 }
}
EOF

# Set cache size to 0 so that each trace request should trigger flush.
$t->write_file('server_config.pb.txt', <<"EOF");
cloud_tracing_config {
  url_override: "http://127.0.0.1:${CloudTracePort}"
  aggregation_config {
    time_millisec: 300
    cache_max_size: 0
  }
  samling_config {
    minimum_qps: 0
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
  endpoints {
    metadata_server http://127.0.0.1:${MetadataPort};
  }
  server {
    listen 127.0.0.1:${NginxPort};
    server_name localhost;
    location / {
      endpoints {
        api service.json;
        server_config server_config.pb.txt;
        on;
      }
      proxy_pass http://127.0.0.1:${BackendPort};
    }
  }
}
EOF

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&metadata, $t, $MetadataPort, 'metadata.log');
$t->run_daemon(\&cloudtrace, $t, $CloudTracePort, 'cloudtrace.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${MetadataPort}"), 1, 'Metadata server socket ready.');
is($t->waitforsocket("127.0.0.1:${CloudTracePort}"), 1, 'Cloud trace socket ready.');

$t->run();

################################################################################

my $trace_id = 'e133eacd437d8a12068fd902af3962d8';
my $parent_span_id = '12345678';
my $response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves HTTP/1.0
Host: localhost
X-Cloud-Trace-Context: ${trace_id}/${parent_span_id};o=1

EOF

my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body, <<'EOF', 'Shelves returned in the response body.');
List of shelves.
EOF

$t->stop();
$t->stop_daemons();

################################################################################

# Verify servicecontrol was not called.
is($t->read_file('servicecontrol.log'), '', 'Service control was not called.');

# Verify cloud trace server was not called.
is($t->read_file('cloudtrace.log'), '', 'Cloud trace server was not called.');

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/shelves', <<'EOF');
HTTP/1.1 200 OK
Connection: close

List of shelves.
EOF

  $server->run();
}

sub servicecontrol {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->run();
}

sub metadata {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  my $response_header = <<'EOF';
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

EOF

  $server->on('GET', '/computeMetadata/v1/?recursive=true',
    $response_header . ApiManager::get_metadata_response_body);

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

sub cloudtrace {
  my ($t, $port, $file, $done) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
      or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->run();
}

################################################################################
