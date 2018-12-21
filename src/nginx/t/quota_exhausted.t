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
use Time::HiRes qw(usleep nanosleep);

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

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(22);

# Save service configuration that disables the report cache.
# Report request will be sent for each client request
$t->write_file('server.pb.txt', <<"EOF");
service_control_config {
  report_aggregator_config {
    cache_entries: 10
    flush_interval_ms: 100
  }
}
EOF

# Save service name in the service configuration protocol buffer file.
$t->write_file( 'service.pb.txt',
    ApiManager::get_bookstore_service_config . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
quota {
 metric_rules [
   {
     selector: "ListShelves"
     metric_costs: [
       {
         key: "metrics_first"
         value: 2
       },
       {
         key: "metrics_second"
         value: 1
       }
     ]
   }
 ]
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
        api service.pb.txt;
        server_config server.pb.txt;
        %%TEST_CONFIG%%
        on;
      }
      proxy_pass http://127.0.0.1:${BackendPort};
    }
  }
}
EOF

my $quota_done = 'quota_done';
my $report_done = 'report_done';

$t->run_daemon( \&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon( \&servicecontrol, $t, $ServiceControlPort,
    'servicecontrol.log', $quota_done, $report_done);
is( $t->waitforsocket("127.0.0.1:${BackendPort}"),
    1, 'Bookstore socket ready.' );
is( $t->waitforsocket("127.0.0.1:${ServiceControlPort}"),
    1, 'Service control socket ready.' );
$t->run();

################################################################################

my $ok_response =
  ApiManager::http_get( $NginxPort, '/shelves?key=this-is-an-api-key' );

# waiting for the first request getting the AllocateQuotaResponse
# before sending the second one
is($t->waitforfile("$t->{_testdir}/${quota_done}"), 1,
  'AllocateQuotaResponse is ready');

my $error_response =
  ApiManager::http_get( $NginxPort, '/shelves?key=this-is-an-api-key' );

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');

$t->stop_daemons();

my ( $ok_response_headers, $ok_response_body ) = split /\r\n\r\n/, $ok_response, 2;
like( $ok_response_headers, qr/HTTP\/1\.1 200 OK/,
  'First request returns HTTP 200 OK.' );
is( $ok_response_body, <<'EOF', 'Shelves returned in the response body.' );
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

my ( $error_response_headers, $error_response_body ) = split /\r\n\r\n/,
  $error_response, 2;
like( $error_response_headers, qr/HTTP\/1\.1 429/,
  'Second request returns HTTP 429.' );
is( $error_response_body, <<'EOF', 'Shelves returned in the response body.' );
{
 "code": 8,
 "message": "Insufficient tokens for quota group and limit 'apiWriteQpsPerProject_LOW' of service 'test.appspot.com', using the limit by ID 'container:123123'.",
 "details": [
  {
   "@type": "type.googleapis.com/google.rpc.DebugInfo",
   "stackEntries": [],
   "detail": "internal"
  }
 ]
}
EOF

# Backend will got 1 request for the first request. The second one will
# be blocked by ESP for exhausted qutoa
my @requests = ApiManager::read_http_stream( $t, 'bookstore.log' );
is( scalar @requests, 1, 'Backend received empty request' );

@requests = ApiManager::read_http_stream( $t, 'servicecontrol.log' );
is( scalar @requests, 3, 'Service control received four requests' );

# :check triggered by the first request and cached, the second request read from
# cache
my $r = shift @requests;
is( $r->{verb}, 'POST', ':check verb was post' );
is( $r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check',
  ':check was called');
is( $r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}",
  'Host header was set');
is( $r->{headers}->{'content-type'}, 'application/x-protobuf',
  ':check Content-Type was protocol buffer');

# :allocateQuota request triggered by cache refresh module, then cached.
# the second request read from the cache
$r = shift @requests;
is( $r->{verb}, 'POST', ':allocateQuota verb was post' );
is( $r->{uri},
  '/v1/services/endpoints-test.cloudendpointsapis.com:allocateQuota',
  ':allocateQuota was called');
is( $r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}",
  'Host header was set');
is( $r->{headers}->{'content-type'}, 'application/x-protobuf',
  ':allocateQuota Content-Type was protocol buffer' );

# check report from the cache flush
$r = shift @requests;

is( $r->{verb}, 'POST', ':report verb was post' );
is( $r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:report',
  ':report was called');
is( $r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}",
  'Host header was set');
is( $r->{headers}->{'content-type'}, 'application/x-protobuf',
  ':report Content-Type was protocol buffer' );

################################################################################

sub bookstore {
    my ( $t, $port, $file ) = @_;
    my $server = HttpServer->new( $port, $t->testdir() . '/' . $file )
      or die "Can't create test server socket: $!\n";
    local $SIG{PIPE} = 'IGNORE';

    $server->on( 'GET', '/shelves?key=this-is-an-api-key', <<'EOF');
HTTP/1.1 200 OK
Connection: close

{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF
    $server->run();
}

my @quota_responses = ();
my $quota_response_index = 0;

sub servicecontrol {
    my ( $t, $port, $file, $quota_done, $report_done ) = @_;
    my $server = HttpServer->new( $port, $t->testdir() . '/' . $file )
      or die "Can't create test server socket: $!\n";
    local $SIG{PIPE} = 'IGNORE';

    my $quota_response_exhausted =
      ServiceControl::convert_proto( <<'EOF', 'quota_response', 'binary' );
operation_id: "006eaa26-5c2f-41bc-b6d8-0972eff8bdf6"
allocate_errors {
  code: RESOURCE_EXHAUSTED
  description: "Insufficient tokens for quota group and limit \'apiWriteQpsPerProject_LOW\' of service \'test.appspot.com\', using the limit by ID \'container:123123\'."
}
service_config_id: "2017-02-08r9"
EOF

    $server->on( 'POST',
        '/v1/services/endpoints-test.cloudendpointsapis.com:check', <<'EOF');
HTTP/1.1 200 OK
Connection: close

EOF


    $server->on_sub('POST',
      '/v1/services/endpoints-test.cloudendpointsapis.com:allocateQuota', sub {
      my ($headers, $body, $client) = @_;
      print $client <<'EOF'.$quota_response_exhausted;
HTTP/1.1 200 OK
Connection: close

EOF
     $t->write_file($quota_done, ':quota done');
  });


  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:report', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
     $t->write_file($report_done, ':report done');
  });

    $server->run();
}

################################################################################
