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
my $ServiceManagementPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(7);

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
  fetch_throttle_window_s: 1
  detect_rollout_interval_s: 1
}
service_config_rollout {
  traffic_percentages { 
    key: "%%TESTDIR%%/service.1.pb.txt"
    value: 80
  }
  traffic_percentages { 
    key: "%%TESTDIR%%/service.2.pb.txt"
    value: 20
  }
  rollout_id: "2017-05-16r0"
}
rollout_strategy: "managed"
EOF

# Save service name in the service configuration protocol buffer file.
$t->write_file( 'service.1.pb.txt',
  ApiManager::get_bookstore_service_config . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

my $service_config_2 = ApiManager::get_bookstore_service_config;
my $find = 'id: "2016-08-25r1"';
my $replace = 'id: "2016-08-25r2"';
$service_config_2 =~ s/$find/$replace/g;

# Save service name in the service configuration protocol buffer file.
$t->write_file( 'service.2.pb.txt', $service_config_2 . <<"EOF");
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

$t->run_daemon( \&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log',
  $report_done);
$t->run_daemon( \&servicemanagement, $t, $ServiceManagementPort,
  'servicemanagement.log', $rollout_done);
  
is( $t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1,
  'Service control socket ready.' );
is( $t->waitforsocket("127.0.0.1:${ServiceManagementPort}"), 1,
  'Service management socket ready.' );
$t->run();

################################################################################

# By not sending any traffic, this test relies on detect_rollout_timer
# to detect rollout changes. Its interval is 1s, it will send an empty report every second
# The report will return the latest rollout id. If it is changed, it will trigger a new
# config fetch.

# waiting for the first report request is done
is($t->waitforfile($t->{_testdir}."/".${report_done} . ".0"), 1, 'Report1 body file ready.');

# waiting for the rolllout update
is($t->waitforfile("$t->{_testdir}/${rollout_done}"), 1, 'Rollout body file ready.');

my $max_retry = 5;
my ($response, $response_headers, $response_body, $endpoints_status);
# verify the the final /endpoints_status
do {
    # wait for the refresh
    sleep(1);

    $response = ApiManager::http_get($NginxPort, '/endpoints_status' );
    ( $response_headers, $response_body ) = split /\r\n\r\n/, $response, 2;
    $endpoints_status = decode_json( $response_body );
} while($max_retry-- > 0 && $endpoints_status->{processes}[0]->{espStatus}[0]->
    {serviceConfigRollouts}->{rolloutId} ne '2016-08-25r1');

like( $response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.' );
is($endpoints_status->{processes}[0]->{espStatus}[0]->{serviceConfigRollouts}->
    {rolloutId}, '2016-08-25r1',
    "Rollout was updated from the service management API" );
is($endpoints_status->{processes}[0]->{espStatus}[0]->{serviceConfigRollouts}->
    {percentages}->{'2016-08-25r3'}, '100', "Rollout 2016-08-25r3 is 100%" );

$t->stop_daemons();


################################################################################

sub servicecontrol {
  my ( $t, $port, $file, $done) = @_;
  my $server = HttpServer->new( $port, $t->testdir() . '/' . $file )
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';
  
  my $index = 0;
  my $report_response = ServiceControl::convert_proto(<<'EOF', 'report_response', 'binary');
{
  "serviceRolloutId": "2016-08-25r1"
}
EOF

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:report', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF' . $report_response;
HTTP/1.1 200 OK
Connection: close

EOF
    $t->write_file($done.".".$index, ':report done');
    $index++;
  });

  $server->run();
}

sub servicemanagement {
  my ( $t, $port, $file, $done) = @_;
  my $server = HttpServer->new( $port, $t->testdir() . '/' . $file )
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('GET', '/v1/services/endpoints-test.cloudendpointsapis.com/rollouts?filter=status=SUCCESS', sub {
    my ($headers, $body, $client) = @_;

    print $client <<'EOF' ;
HTTP/1.1 200 OK
Connection: close

{
  "rollouts": [
    {
      "rolloutId": "2016-08-25r1",
      "createTime": "2017-05-01T22:40:09.884Z",
      "createdBy": "test_user@google.com",
      "status": "SUCCESS",
      "trafficPercentStrategy": {
        "percentages": {
          "2016-08-25r3": 100
        }
      },
      "serviceName": "endpoints-test.cloudendpointsapis.com"
    }
  ]
}
EOF
  });

  my $service_config_3 = ApiManager::get_bookstore_service_config;
  my $find = 'id: "2016-08-25r1"';
  my $replace = 'id: "2016-08-25r3"';
  $service_config_3 =~ s/$find/$replace/g;

  $server->on_sub('GET', '/v1/services/endpoints-test.cloudendpointsapis.com/configs/2016-08-25r3', sub {
    my ($headers, $body, $client) = @_;
    print $client <<"EOF" ;
HTTP/1.1 200 OK
Connection: close

control {
    environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF
    print $client $service_config_3;

    $t->write_file($done, ':rollout done');
  });

  $server->run();
}

################################################################################
