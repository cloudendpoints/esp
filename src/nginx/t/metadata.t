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

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(30);

my $config = ApiManager::get_bookstore_service_config . <<"EOF";
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

$t->write_file('service.pb.txt', $config);

$t->write_file('server_config.pb.txt', <<"EOF" . ApiManager::disable_service_control_cache);
metadata_attributes {
  zone: "us-west1-d"
}
EOF

$t->write_file_expand('nginx.conf', <<"EOF");
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
        server_config server_config.pb.txt;
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

my $shelves = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key');
my $books = ApiManager::http_get($NginxPort,'/shelves/Fiction/books?key=this-is-an-api-key');

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');

$t->stop();
$t->stop_daemons();

my ($shelves_headers, $shelves_body) = split /\r\n\r\n/, $shelves, 2;
like($shelves_headers, qr/HTTP\/1\.1 200 OK/, '/shelves returned HTTP 200.');
is($shelves_body, "Poetry\nFiction\n", '/shelves returned correct response body.');

my ($books_headers, $books_body) = split /\r\n\r\n/, $books, 2;
like($books_headers, qr/HTTP\/1\.1 200 OK/, '/books returned HTTP 200.');
is($books_body, "Never Let Me Go\n1Q84\n", '/books returned correct response body.');

# Check metadata server log.

my @metadata_requests = ApiManager::read_http_stream($t, 'metadata.log');
is(scalar @metadata_requests, 1, 'Metadata server received 1 request.');

# Request 1
my $r = shift @metadata_requests;
is($r->{verb}, 'GET', 'Metadata request 2 was GET');
is($r->{uri}, '/computeMetadata/v1/instance/service-accounts/default/token',
   'Metadata received get service account token');
is($r->{headers}->{'metadata-flavor'}, 'Google', 'Metadata-Flavor header was set on request 2.');
is($r->{headers}->{host}, "127.0.0.1:${MetadataPort}", 'Host header was set on request 2.');

# Check service control log for correct authorization bearer token.

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 4, 'Service control received 4 requests.');

# Request 1 :check 1
$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':check 1 was POST');
is($r->{path}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', ':check 1 path');
is($r->{headers}->{authorization}, 'Bearer ya29.7gFRTEGmovWacYDnQIpC9X9Qp8cH0sgQyWVrZaB1Eg1WoAhQMSG4L2rtaHk1',
   ':check 1 was authenticated');

# Request 2 : report 1
$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':report 1 was POST');
is($r->{path}, '/v1/services/endpoints-test.cloudendpointsapis.com:report', ':report 1 path');
is($r->{headers}->{authorization}, 'Bearer ya29.7gFRTEGmovWacYDnQIpC9X9Qp8cH0sgQyWVrZaB1Eg1WoAhQMSG4L2rtaHk1',
   ':report 1 was authenticated');

my $report_json = decode_json(ServiceControl::convert_proto($r->{body}, 'report_request', 'json'));
my $operations = $report_json->{operations};
is(@{$operations}, 1, 'There are 1 operations total');
is('us-west1-d', $operations->[0]->{labels}->{'cloud.googleapis.com/location'}, 'Operation 1 has correct zone');

# Request 3: check 2
$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':check 2 was POST');
is($r->{path}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', ':check 2 path');
is($r->{headers}->{authorization}, 'Bearer ya29.7gFRTEGmovWacYDnQIpC9X9Qp8cH0sgQyWVrZaB1Eg1WoAhQMSG4L2rtaHk1',
   ':check 2 was authenticated');

# Request 4 : report 2
$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':report 2 was POST');
is($r->{path}, '/v1/services/endpoints-test.cloudendpointsapis.com:report', ':report 2 path');
is($r->{headers}->{authorization}, 'Bearer ya29.7gFRTEGmovWacYDnQIpC9X9Qp8cH0sgQyWVrZaB1Eg1WoAhQMSG4L2rtaHk1',
   ':report 2 was authenticated');

$report_json = decode_json(ServiceControl::convert_proto($r->{body}, 'report_request', 'json'));
$operations = $report_json->{operations};
is(@{$operations}, 1, 'There are 1 operations total');
is('us-west1-d', $operations->[0]->{labels}->{'cloud.googleapis.com/location'}, 'Operation 1 has correct zone');

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/shelves?key=this-is-an-api-key', <<'EOF');
HTTP/1.1 200 OK
Content-Type: application/text
Connection: close

Poetry
Fiction
EOF

  $server->on('GET', '/shelves/Fiction/books?key=this-is-an-api-key', <<'EOF');
HTTP/1.1 200 OK
Content-Type: application/text
Connection: close

Never Let Me Go
1Q84
EOF

  $server->run();
}

sub servicecontrol {
  my ($t, $port, $file, $done) = @_;

  # Save requests (last argument).
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:report', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
    $t->write_file($done, ':report done');
  });

  $server->run();
}

sub metadata {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

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

################################################################################
