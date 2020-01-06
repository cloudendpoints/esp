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
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework
use JSON::PP;
use MIME::Base64;

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $PubkeyPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(15);

my $config = ApiManager::get_bookstore_service_config;
$config .= <<"EOF";
authentication {
  providers {
    id: "test_auth"
    issuer: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l\@developer.gserviceaccount.com"
    jwks_uri: "http://127.0.0.1:${PubkeyPort}/pubkey"
  }
  rules {
    selector: "ListShelves"
    requirements {
      provider_id: "test_auth"
    }
  }
}
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

$t->write_file('service.pb.txt', $config);
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
        on;
      }
      proxy_pass http://127.0.0.1:${BackendPort};
    }
  }
}
EOF

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&pubkey, $t, $PubkeyPort, 'pubkey.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service Control ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');

$t->run();

################################################################################

my $token = Auth::get_auth_token('./src/nginx/t/matching-client-secret.json');
my $response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
Authorization: Bearer $token

EOF
my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body, <<'EOF', 'Shelves returned in the response body.');
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

$t->stop_daemons();

my @bookstore_requests = ApiManager::read_http_stream($t, 'bookstore.log');
is(scalar @bookstore_requests, 1, 'Bookstore received one request');

# Check backend request.
my $r = shift @bookstore_requests;
is($r->{verb}, 'GET', 'backend received a get');
is($r->{path}, '/shelves', 'backend received get /shelves');
isnt($r->{headers}->{authorization}, undef, 'Authorization header was received');
isnt($r->{headers}->{'x-endpoint-api-userinfo'}, undef, 'X-Endpoint-API-UserInfo was received');

# Check X-Endpoint-API-UserInfo header.
my $user_info = $r->{headers}->{'x-endpoint-api-userinfo'};
my $user_info_json =  decode_json(decode_base64($user_info));
# Check the claims
isnt($user_info_json->{'claims'}, undef, 'claims were received');
my $claims =  decode_json($user_info_json->{'claims'});
is($claims->{iss}, '628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com', 'iss field in the claims was as expected');
is($claims->{sub}, '628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com', 'sub field in the claims was as expected');
is($claims->{aud}, 'endpoints-test.cloudendpointsapis.com', 'aud field in the claims was as expected');
# Remove the ever-changing iat and exp claims before comparing with the expected values.
delete $user_info_json->{'claims'};
$user_info = encode_base64(encode_json($user_info_json));

my $expected_user_info = {
   'issuer' => '628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com',
   'id' => '628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com',
   'audiences' => ["endpoints-test.cloudendpointsapis.com"],
};
ok(ApiManager::compare_user_info($user_info, $expected_user_info), 'Passed user_info is correct.');

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/shelves?key=this-is-an-api-key', <<'EOF');
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

sub servicecontrol {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', <<'EOF');
HTTP/1.1 200 OK
Connection: close

EOF
  $server->run();
}

sub pubkey {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/pubkey', <<'EOF');
HTTP/1.1 200 OK
Connection: close

{
 "keys": [
  {
   "kty": "RSA",
   "alg": "RS256",
   "use": "sig",
   "kid": "62a93512c9ee4c7f8067b5a216dade2763d32a47",
   "n": "0YWnm_eplO9BFtXszMRQNL5UtZ8HJdTH2jK7vjs4XdLkPW7YBkkm_2xNgcaVpkW0VT2l4mU3KftR-6s3Oa5Rnz5BrWEUkCTVVolR7VYksfqIB2I_x5yZHdOiomMTcm3DheUUCgbJRv5OKRnNqszA4xHn3tA3Ry8VO3X7BgKZYAUh9fyZTFLlkeAh0-bLK5zvqCmKW5QgDIXSxUTJxPjZCgfx1vmAfGqaJb-nvmrORXQ6L284c73DUL7mnt6wj3H6tVqPKA27j56N0TB1Hfx4ja6Slr8S4EB3F1luYhATa1PKUSH8mYDW11HolzZmTQpRoLV8ZoHbHEaTfqX_aYahIw",
   "e": "AQAB"
  },
  {
   "kty": "RSA",
   "alg": "RS256",
   "use": "sig",
   "kid": "b3319a147514df7ee5e4bcdee51350cc890cc89e",
   "n": "qDi7Tx4DhNvPQsl1ofxxc2ePQFcs-L0mXYo6TGS64CY_2WmOtvYlcLNZjhuddZVV2X88m0MfwaSA16wE-RiKM9hqo5EY8BPXj57CMiYAyiHuQPp1yayjMgoE1P2jvp4eqF-BTillGJt5W5RuXti9uqfMtCQdagB8EC3MNRuU_KdeLgBy3lS3oo4LOYd-74kRBVZbk2wnmmb7IhP9OoLc1-7-9qU1uhpDxmE6JwBau0mDSwMnYDS4G_ML17dC-ZDtLd1i24STUw39KH0pcSdfFbL2NtEZdNeam1DDdk0iUtJSPZliUHJBI_pj8M-2Mn_oA8jBuI8YKwBqYkZCN1I95Q",
   "e": "AQAB"
  }
 ]
}
EOF

  $server->run();
}
