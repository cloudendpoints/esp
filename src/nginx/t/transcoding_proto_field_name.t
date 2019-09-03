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
my $ServiceControlPort = ApiManager::pick_port();
my $GrpcServerPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(10);

$t->write_file('service.pb.txt',
  ApiManager::get_transcoding_test_service_config(
    'endpoints-transcoding-test.cloudendpointsapis.com',
    "http://127.0.0.1:${ServiceControlPort}"));

$t->write_file('server_config.pb.txt', <<EOF);
experimental {
  preserve_proto_field_names: false
}
EOF

$t->write_file_expand('nginx.conf', <<EOF);
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
        server_config server_config.pb.txt;
        on;
      }
      grpc_pass 127.0.0.1:${GrpcServerPort};
    }
  }
}
EOF

$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'servicecontrol.log');
ApiManager::run_transcoding_test_server($t, 'backend.log', "127.0.0.1:${GrpcServerPort}");

is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, "Service control socket ready.");
is($t->waitforsocket("127.0.0.1:${GrpcServerPort}"), 1, "GRPC test backend socket ready.");
$t->run();
is($t->waitforsocket("127.0.0.1:${NginxPort}"), 1, "Nginx socket ready.");

################################################################################

my $response1 = ApiManager::http($NginxPort,<<EOF);
POST /echoshelfid?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 20

{ "shelf_id" : 100 }
EOF

my $response2 = ApiManager::http($NginxPort,<<EOF);
POST /echoshelfid?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 20

{ "shelfId" : 100 }
EOF

ok(ApiManager::verify_http_json_response($response1,
    {'shelfId'=>'100', }), "The shelfId was echoed from shelf_id successfully.");
ok(ApiManager::verify_http_json_response($response2,
    {'shelfId'=>'100', }), "The shelfId was echoed from shelfId successfully.");

$t->stop();
$t->stop_daemons();

# turn on the flag
$t->write_file('server_config.pb.txt', <<EOF);
experimental {
  preserve_proto_field_names: true
}
EOF

$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'servicecontrol.log');
ApiManager::run_transcoding_test_server($t, 'backend.log', "127.0.0.1:${GrpcServerPort}");

is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, "Service control socket ready.");
is($t->waitforsocket("127.0.0.1:${GrpcServerPort}"), 1, "GRPC test backend socket ready.");
$t->run();
is($t->waitforsocket("127.0.0.1:${NginxPort}"), 1, "Nginx socket ready.");

my $response3 = ApiManager::http($NginxPort,<<EOF);
POST /echoshelfid?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 20

{ "shelf_id" : 100 }
EOF

my $response4 = ApiManager::http($NginxPort,<<EOF);
POST /echoshelfid?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 20

{ "shelfId" : 100 }
EOF

ok(ApiManager::verify_http_json_response($response3,
    {'shelf_id'=>'100', }), "The shelf_id was echoed from shelf_id successfully.");
ok(ApiManager::verify_http_json_response($response4,
    {'shelf_id'=>'100', }), "The shelf_id was echoed from shelfId successfully.");


################################################################################

sub service_control {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";

  $server->on_sub('POST', '/v1/services/endpoints-transcoding-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<EOF;
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/endpoints-transcoding-test.cloudendpointsapis.com:report', sub {
    my ($headers, $body, $client) = @_;
    print $client <<EOF;
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
  });

  $server->run();
}

