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
use JSON::PP;
use Data::Dumper;
use Time::HiRes qw(usleep);

################################################################################

use src::nginx::t::ApiManager;    # Must be first (sets up import path to
                                  # the Nginx test module)
use src::nginx::t::HttpServer;
use src::nginx::t::ServiceControl;
use Test::Nginx;    # Imports Nginx's test module
use Test::More;     # And the test framework

################################################################################

# Port assignments
my $NginxPort          = ApiManager::pick_port();
my $BackendPort        = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $ServiceManagementPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(204);

# Save service configuration that disables the report cache.
# Report request will be sent for each client request
$t->write_file_expand('server.pb.txt', <<"EOF");
service_control_config {
  report_aggregator_config {
    cache_entries: 0
    flush_interval_ms: 1000
  }
  check_aggregator_config {
  	cache_entries: 0
  	flush_interval_ms: 1000
  }
}
service_management_config {
  url: "http://127.0.0.1:${ServiceManagementPort}"
  refresh_interval_ms: 100
}
service_config_rollout {
  traffic_percentages { 
    key: "%%TESTDIR%%/service.pb.txt"
    value: 100
  }
  rollout_id: "2017-05-16r0"
}
rollout_strategy: "managed"
EOF

# Save service name in the service configuration protocol buffer file.
$t->write_file( 'service.pb.txt',
  ApiManager::get_bookstore_service_config . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

ApiManager::write_file_expand( $t, 'nginx.conf', <<"EOF");
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
        server_config server.pb.txt;
        %%TEST_CONFIG%%
        on;
      }
      proxy_pass http://127.0.0.1:${BackendPort};
    }
    location /endpoints_status {
      endpoints_status;
      access_log off;
    }
  }
}
EOF

my $report_done = 'report_done';
my $rollout_done = 'rollout_done';

$t->run_daemon( \&bookstore, $t, $BackendPort, 'bookstore.log' );
$t->run_daemon( \&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log',
  $report_done);
$t->run_daemon( \&servicemanagement, $t, $ServiceManagementPort,
  'servicemanagement.log', $rollout_done);
  
is( $t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.' );
is( $t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1,
  'Service control socket ready.' );
is( $t->waitforsocket("127.0.0.1:${ServiceManagementPort}"), 1,
  'Service management socket ready.' );
$t->run();

################################################################################

for (my $count = 0 ; $count < 100 ; $count++) {
    # send the request
    my $response = ApiManager::http_get($NginxPort, '/shelves?key=this-is-an-api-key' );

    # verify the response
    my ( $response_headers, $response_body ) = split /\r\n\r\n/, $response, 2;
    like( $response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.' );
    is( $response_body, <<'EOF', 'Shelves returned in the response body.' );
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF
    # Refresh interval is 0.1s and there should have 10 timeout during 1s.
    # Send 100 requests with 0.01 interval, total is 1s.
    # For each refresh period (0.1s), there should have 10 requests with
    # 10 reports (report batch is disabled), so each refresh interval
    # should have fresh rollout id
    usleep(10);
}

# Since Report response ID is set, serviemanagement server should not be called.
my @servicemanagement_requests = ApiManager::read_http_stream( $t, 'servicemanagement.log' );
is( scalar @servicemanagement_requests, 0, 'Service management is not called' );

################################################################################

sub bookstore {
  my ( $t, $port, $file) = @_;
  my $server = HttpServer->new( $port, $t->testdir() . '/' . $file )
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';
  
    $server->on_sub('POST', '/shelves?key=this-is-an-api-key', sub {
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
  
  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  my $proto_response = ServiceControl::convert_proto(<<'EOF', 'report_response', 'binary');
{
  "serviceRolloutId": "2017-05-16r0"
}
EOF
  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:report', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF' . $proto_response;
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->run();
}

sub servicemanagement {
  my ( $t, $port, $file) = @_;
  my $server = HttpServer->new( $port, $t->testdir() . '/' . $file )
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->run();
}

################################################################################
