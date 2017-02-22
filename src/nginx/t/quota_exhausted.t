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

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(6);

# Save servce configuration that disables the report cache.
# Report request will be sent for each client request
$t->write_file('server.pb.txt', <<"EOF");
service_control_config {
  report_aggregator_config {
    cache_entries: 0
    flush_interval_ms: 1000
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

$t->run_daemon( \&bookstore, $t, $BackendPort, 'bookstore.log' );
$t->run_daemon( \&servicecontrol, $t, $ServiceControlPort,
    'servicecontrol.log' );
is( $t->waitforsocket("127.0.0.1:${BackendPort}"),
    1, 'Bookstore socket ready.' );
is( $t->waitforsocket("127.0.0.1:${ServiceControlPort}"),
    1, 'Service control socket ready.' );
$t->run();

################################################################################

my $response =
  ApiManager::http_get( $NginxPort, '/shelves?key=this-is-an-api-key' );

$t->stop_daemons();

my ( $response_headers, $response_body ) = split /\r\n\r\n/, $response, 2;

like( $response_headers, qr/HTTP\/1\.1 429/, 'Returned HTTP 429.' );
is( $response_body, <<'EOF', 'Shelves returned in the response body.' );
{
 "code": 8,
 "message": "Quota allocation failed.",
 "details": [
  {
   "@type": "type.googleapis.com/google.rpc.DebugInfo",
   "stackEntries": [],
   "detail": "internal"
  }
 ]
}
EOF

my @requests = ApiManager::read_http_stream( $t, 'bookstore.log' );
is( scalar @requests, 0, 'Backend received empty request' );


@requests = ApiManager::read_http_stream( $t, 'servicecontrol.log' );
is( scalar @requests, 3, 'Service control received three requests' );


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
    my ( $t, $port, $file ) = @_;
    my $server = HttpServer->new( $port, $t->testdir() . '/' . $file )
      or die "Can't create test server socket: $!\n";
    local $SIG{PIPE} = 'IGNORE';

    my $quota_response_exhausted =
      ServiceControl::convert_proto( <<'EOF', 'quota_response', 'binary' );
operation_id: "006eaa26-5c2f-41bc-b6d8-0972eff8bdf6"
allocate_errors {
  code: RESOURCE_EXHAUSTED
  description: "Insufficient tokens for quota group and limit \'apiWriteQpsPerProject_LOW\' of service \'jaebonginternal.sandbox.google.com\', using the limit by ID \'container:1002409420961\'."
}
service_config_id: "2017-02-08r9"
EOF

    $server->on( 'POST',
        '/v1/services/endpoints-test.cloudendpointsapis.com:check', <<'EOF');
HTTP/1.1 200 OK
Connection: close

EOF

    $server->on('POST',
      '/v1/services/endpoints-test.cloudendpointsapis.com:allocateQuota',
      <<'EOF' . $quota_response_exhausted);
HTTP/1.1 200 OK
Connection: close

EOF

  $server->on( 'POST',
    '/v1/services/endpoints-test.cloudendpointsapis.com:report', <<'EOF');
HTTP/1.1 200 OK
Connection: close

EOF

    $server->run();
}

################################################################################
