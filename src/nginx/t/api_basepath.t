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
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(27);

# Save service name in the service configuration protocol buffer file.

$t->write_file('service.pb.txt', ApiManager::get_bookstore_service_config . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

$t->write_file('server.pb.txt', <<"EOF");
service_control_config {
  check_aggregator_config {
    cache_entries: 0
    flush_interval_ms: 100
  }
  report_aggregator_config {
    cache_entries: 10
    flush_interval_ms: 100
  }
}
api_service_config {
  base_path: "/api"
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
        %%TEST_CONFIG%%
        server_config server.pb.txt;
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

my $response_first = ApiManager::http_get($NginxPort,'/api/shelves?key=this-is-an-api-key');
my $response_second = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key');

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');


$t->stop_daemons();

my ($response_headers, $response_body) = split /\r\n\r\n/, $response_first, 2;

like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body, <<'EOF', 'Shelves returned in the response body.');
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

($response_headers, $response_body) = split /\r\n\r\n/, $response_second, 2;

like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body, <<'EOF', 'Shelves returned in the response body.');
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

my @requests = ApiManager::read_http_stream($t, 'bookstore.log');
is(scalar @requests, 2, 'Backend received two requests');

my $r = shift @requests;
is($r->{verb}, 'GET', 'Backend request was a get');
is($r->{uri}, '/shelves?key=this-is-an-api-key', 'Backend uri was /shelves');
is($r->{headers}->{host}, "127.0.0.1:${BackendPort}", 'Host header was set');

$r = shift @requests;
is($r->{verb}, 'GET', 'Backend request was a get');
is($r->{uri}, '/shelves?key=this-is-an-api-key', 'Backend uri was /shelves');
is($r->{headers}->{host}, "127.0.0.1:${BackendPort}", 'Host header was set');



@requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @requests, 3, 'Service control received one request');

$r = shift @requests;
is($r->{verb}, 'POST', ':check verb was post');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', ':check was called');
is($r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}", 'Host header was set');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', ':check Content-Type was protocol buffer');


$r = shift @requests;
is($r->{verb}, 'POST', ':check verb was post');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', ':check was called');
is($r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}", 'Host header was set');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', ':check Content-Type was protocol buffer');


$r = shift @requests;
is($r->{verb}, 'POST', ':check verb was post');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:report', ':check was called');
is($r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}", 'Host header was set');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', ':report Content-Type was protocol buffer');


################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('GET', '/shelves?key=this-is-an-api-key',sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF
  });

  $server->run();
}

sub servicecontrol {
  my ( $t, $port, $file, $done) = @_;
  my $server = HttpServer->new( $port, $t->testdir() . '/' . $file )
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub( 'POST',
    '/v1/services/endpoints-test.cloudendpointsapis.com:check', sub {
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
