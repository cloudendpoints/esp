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

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $PubkeyPort = ApiManager::pick_port();
my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(13);

my $config = ApiManager::get_bookstore_service_config;
$config .= <<"EOF";
authentication {
  providers [
    {
      id: "test_auth"
      issuer: "es256-issuer"
      jwks_uri: "http://127.0.0.1:${PubkeyPort}/key"
    },
    {
      id: "test_auth_1"
      issuer: "rs256-issuer"
      jwks_uri: "http://127.0.0.1:${PubkeyPort}/key"
    }
  ]
  rules {
    selector: "ListShelves"
    requirements [
      {
        provider_id: "test_auth"
        audiences: "ok_audience"
      },
      {
        provider_id: "test_auth_1"
        audiences: "ok_audience"
      }
    ]
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

# Two public keys are used, one for ES256 and the other for RS256.
# ESP will identify and use the proper key according to kid if
# existing. If not, ESP simply tries all keys to find the right one.
# Please see jwk_generator.py under /src/tools for jwk generation.
my $pubkeys = <<'EOF';
{
 "keys": [
  {
   "kty": "EC",
   "crv": "P-256",
   "x": "lqldKduURoauGtQskOXRTTociai06C-Ug_lwDqcXdd4",
   "y": "t3FPM5-BhLsjyTG6QcDkTotU6PTMmrT6KCfr4L_0Lhk",
   "alg": "ES256",
   "kid": "1a"
  },
  {
   "kty": "RSA",
   "n": "zaS0LKbCovc6gdmwwEbovLBqEuat2ihKmuXMEAh7yjk--Pw55djgkpiAFaoTr0-iEnJB8QKQAkssU5mQcKHCtKRfVH9TZv3JC8mXeSg1dvS-AckkGqXwuPpYyaTUDZsd7u3xW3lSX4QtrLNcwCo0TRFmUGcpkecy6omJdD8kwhWXYOEkDPZqZXlvWkLfyuelWE8Wcrv-X_v8UrCMOOECRPRxl5tmC93vMnZZAHN35gyLizaPOkXPR69DN-_d34aiLctphiqzTJUlMlpIU2SciXj2CaOMFzioy-cRb9sbr8eN91cDPDs4r-EiFB6bcoAJxaHCyxdhJYihFGfwGjhCkQ",
   "e": "AQAB",
   "alg": "RS256",
   "kid": "2b"
  }
 ]
}
EOF

# es256_token is issued by "es256-issuer". Please see jwt_generator.py
# under /src/tools/ for ES256-signed jwt token generation.
my $es256_token = "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6IjFhIn0.".
"eyJpc3MiOiJlczI1Ni1pc3N1ZXIiLCJzdWIiOiJlczI1Ni1pc3N1ZXIiLCJhdWQiOiJva1".
"9hdWRpZW5jZSJ9.hz9IUedX6WTbuxQSbcXBSKfvF2hK48o06CnxJn-5vyOkWfUNroJjb3J".
"okQpweF9XFI8RxeMGPKFMdHb8qyIlqA";

# rs256_token is issued by "rs256-issuer" to avoid pkey cache
# conflict from second case to third case. Please see jwt_generator.py
# under /src/tools/ for RS256-signed jwt token generation.
my $rs256_token = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6IjJiIn0.".
"eyJpc3MiOiJyczI1Ni1pc3N1ZXIiLCJzdWIiOiJyczI1Ni1pc3N1ZXIiLCJhdWQiOiJva1".
"9hdWRpZW5jZSJ9.Idf-XyipQCoMmIkI8TT3LgHUseV5AG-tJGhGrEldto-q44oNz9ZEd3K".
"oJ3TlZGKknfEfaSCndsFR_yeHrI1CLdQ7kIs2SaRQP2aG4QqJAwn0-kFoTSUwxqQtV428A".
"KrMrTeahu6ZGOGqwaLMOKP2F7pzI2sCFAYMwLCLhbHzvzRwhIPekG8iENj5YDS5_C5GtFt".
"UV4iL7e6KS7ZqRTljqZB6HUjG7TL_QMZuQ7S44bLGePgx8AeMlEqBzFizG7cJKvGJjsSTi".
"uxvESBnPNpjm4bNFLgLXULoRsoXgU3i1DKQ0r12uztARJpq79diXf-ln7tV-TCwOXlubbb".
"2hiP6-A";

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&key, $t, $PubkeyPort, $pubkeys, 'key.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');

$t->run();

################################################################################
# No JWT passed in.

my $response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost

EOF

like($response, qr/HTTP\/1\.1 401 Unauthorized/, 'Returned HTTP 401, missing creds.');
like($response, qr/WWW-Authenticate: Bearer/, 'Returned auth challenge.');
like($response, qr/Content-Type: application\/json/i,
     'Missing creds returned application/json body.');
like($response, qr/JWT validation failed: Missing or invalid credentials/i,
     "Error body contains 'Missing or invalid credentials'.");

################################################################################
# ES256-signed jwt token is passed in "X-Goog-Iap-Jwt-Assertion" header.

$response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
X-Goog-Iap-Jwt-Assertion: $es256_token

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

################################################################################
# RS256-signed jwt token is passed in "Authorization: Bearer" header.

$response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
Authorization: Bearer $rs256_token

EOF

($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;
like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body, <<'EOF', 'Shelves returned in the response body.');
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

################################################################################
# RS256-signed jwt token is passed in query.

$response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key&access_token=$rs256_token HTTP/1.0
Host: localhost

EOF

($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;
like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body, <<'EOF', 'Shelves returned in the response body.');
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

################################################################################

$t->stop_daemons();
$t->stop();

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('GET', '/shelves', sub {
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
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->run();
}

sub key {
  my ($t, $port, $keys, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('GET', '/key', sub {
    my ($headers, $body, $client) = @_;
    print $client <<"EOF";
HTTP/1.1 200 OK
Connection: close

$keys
EOF
  });

  $server->run();
}

