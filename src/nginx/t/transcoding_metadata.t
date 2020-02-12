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
use src::nginx::t::Auth;
use src::nginx::t::HttpServer;
use src::nginx::t::ServiceControl;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework
use MIME::Base64;
use JSON::PP;

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $GrpcServerPort = ApiManager::pick_port();
my $PubkeyPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(15);

$t->write_file('service.pb.txt',
        ApiManager::get_grpc_test_service_config($GrpcServerPort) .
        ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
authentication {
  providers {
    id: "test_auth"
    issuer: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l\@developer.gserviceaccount.com"
    jwks_uri: "http://127.0.0.1:${PubkeyPort}/pubkey"
  }
  rules {
    selector: "test.grpc.Test.Echo"
    requirements {
      provider_id: "test_auth"
      audiences: "ok_audience_1,ok_audience_2"
    }
  }
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
        on;
      }
      grpc_pass 127.0.0.1:${GrpcServerPort};
    }
  }
}
EOF

my $pkey_jwk = Auth::get_public_key_jwk;
my $auth_token = Auth::get_auth_token('./src/nginx/t/matching-client-secret.json', 'ok_audience_1');

$t->run_daemon(\&pubkey, $t, $PubkeyPort, $pkey_jwk, 'pubkey.log');
$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&ApiManager::grpc_test_server, $t, "127.0.0.1:${GrpcServerPort}");

is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, "Service control socket ready.");
is($t->waitforsocket("127.0.0.1:${GrpcServerPort}"), 1, "GRPC test server socket ready.");
$t->run();
is($t->waitforsocket("127.0.0.1:${NginxPort}"), 1, "Nginx socket ready.");

################################################################################

my $request = <<EOF;
{
  "returnInitialMetadata" : {
    "initial-text": "dGV4dA==",
    "initial-binary-bin": "YmluYXJ5"
  }
}
EOF

my $content_length = length($request);

my $response = ApiManager::http($NginxPort,<<EOF . $request);
POST /echo?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Authorization: Bearer $auth_token
User-Agent: original-user-agent
Content-Type: application/json
Content-Length: $content_length
client-text: text
client-binary-bin: YmluYXJ5

EOF

$t->stop_daemons();

my ($headers, $actual_body) = split /\r\n\r\n/, $response, 2;

my $json_response = decode_json($actual_body);

# EachResponse.received_metadata is a map<string, bytes>,
# when transcoding EachResponse to JSON, "bytes" data type is base64 encoded.
# 'dGV4dA==' is base64('text')
is('dGV4dA==', $json_response->{receivedMetadata}->{'client-text'}, "Received client-text metadata");
is('YmluYXJ5', $json_response->{receivedMetadata}->{'client-binary-bin'}, "Received client-binary metadata");

# 'User-Agent' has been forwarded to grpc server as 'x-forwarded-user-agent'
# 'b3JpZ2luYWwtdXNlci1hZ2VudA==' is base64('original-user-agent')
is('b3JpZ2luYWwtdXNlci1hZ2VudA==', $json_response->{receivedMetadata}->{'x-forwarded-user-agent'}, "Received x-forwarded-user-agent metadata");

# Check X-Endpoint-API-UserInfo header.
my $user_info = $json_response->{receivedMetadata}->{'x-endpoint-api-userinfo'};
my $user_info_json =  decode_json(decode_base64(decode_base64($user_info)));
# Check the issuer and id
is($user_info_json->{issuer}, '628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com', 'issuer field was as expected');
is($user_info_json->{id}, '628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com', 'id field was as expected');
# Check the claims
isnt($user_info_json->{'claims'}, undef, 'claims were received');
my $claims =  decode_json($user_info_json->{'claims'});
is($claims->{iss}, '628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com', 'iss field in the claims was as expected');
is($claims->{sub}, '628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com', 'sub field in the claims was as expected');
is($claims->{aud}, 'ok_audience_1', 'aud field in the claims was as expected');


my $api_key = decode_base64($json_response->{receivedMetadata}->{'x-api-key'});
is($api_key, 'api-key', 'api-key passed by query param is set in header');

like($headers, qr/initial-text: text/, "Server returns initial text metadata");
like($headers, qr/initial-binary-bin: YmluYXJ5/, "Server returns initial binary metadata");


################################################################################

sub service_control {
  my ($t, $port, $file) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";

  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('POST', '/v1/services/endpoints-grpc-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<EOF;
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
  });

  $server->run();
}

################################################################################

sub pubkey {
  my ($t, $port, $pkey, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/pubkey', <<"EOF");
HTTP/1.1 200 OK
Connection: close

$pkey
EOF

  $server->run();
}

