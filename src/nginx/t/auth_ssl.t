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

# Tests certificate check with outgoing HTTPS connections.

# Port assignments
my $NginxPort = 8080;
my $BackendPort = 8081;
my $ServiceControlPort = 8082;
my $PubkeyPort = 8083;
my $OpenIdPort = 8085;
my $OpenIdSslPort = $OpenIdPort + 443;

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(30);

my $config = ApiManager::get_bookstore_service_config;
$config .= <<"EOF";
authentication {
  providers: [
   {
     id: "test_auth"
     issuer: "https://127.0.0.1:${OpenIdSslPort}"
   }
  ],
  rules: {
    selector: "ListShelves"
    requirements: [
     {
       provider_id: "test_auth"
     }
    ]
  }
}
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

$t->write_file('service.pb.txt', $config);
$t->write_file_expand('nginx.conf', <<"EOF");
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
      proxy_pass http://127.0.0.1:${BackendPort};
    }
  }
}
EOF

# Token generated with the following header and payload, using private key from matching-client-secret.json.
# Header:
# {
#   "alg": "RS256",
#   "typ": "JWT",
#   "kid": "b3319a147514df7ee5e4bcdee51350cc890cc89e"
# }
# Payload:
# {
#   "iss": "http://127.0.0.1:${OpenIdPort}",
#   "sub": "end-user-id",
#   "aud": "endpoints-test.cloudendpointsapis.com",
#   "iat": 1461779321,
#   "exp": 2461782921
# }
my $token = 'eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6ImIzMzE5YTE0NzUxNGRmN2VlNWU0YmNkZWU1MTM1MGNjODkwY2M4OWUifQ.eyJpc3MiOiJodHRwczovLzEyNy4wLjAuMTo4NTI4Iiwic3ViIjoiZW5kLXVzZXItaWQiLCJhdWQiOiJlbmRwb2ludHMtdGVzdC5jbG91ZGVuZHBvaW50c2FwaXMuY29tIiwiaWF0IjoxNDYxNzc5MzIxLCJleHAiOjI0NjE3ODI5MjF9.WJLhOPqFtoisyW5qggeqGGC7JQsGQUN-v6lzaG3HLZr8fSlahycwZKbGLM4ehVbrJqEahMAADg4YicyGxBvI_va-uE_d1iEtsvXbIJd26uZO0H_8DLmhn_Ku4SAIsGoKzw2R9cHgDrxbxuIxxuMRJSLTjeOgLAbSyHe75P5h_Y1Vuv0egTMZ56lKFKzh87Zfqf8FJNbR7UbX5RuAgxJunE8xwXYb8mq0a0Dz94LVfG_MwHwxlSOG4JH8JdmNl0iSGtR8MJxw-mQAPOrFWOykFn65HS8z9JF80L-09Ax8_nECvzTWQJNwvURSiKGEzzPB_8Wh4jwM_4HCYUEE3fpJog';

sub success_test {
  $t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
  $t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
  $t->run_daemon(\&pubkey, $t, $PubkeyPort, Auth::get_public_key_jwk, 'pubkey.log');
  $t->run_daemon(\&openid, $t, $OpenIdPort, 'openid.log');
  is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
  is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
  is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');
  is($t->waitforsocket("127.0.0.1:${OpenIdPort}"), 1, "openid socket (${OpenIdPort}) ready");
  $t->run();

  # Positive test case: Successfully completes the request via openID discovery.
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

  my @openid_requests = ApiManager::read_http_stream($t, 'openid.log');
  is(scalar @openid_requests, 1, 'OpenID was called once');
  my $r = shift @openid_requests;
  is($r->{verb}, 'GET', 'OpenID request was a get');
  is($r->{uri}, '/.well-known/openid-configuration',
     'OpenID discovery doc was retrieved');

  my @pubkey_requests = ApiManager::read_http_stream($t, 'pubkey.log');
  is(scalar @pubkey_requests, 1, 'Pubkey was called once.');
  $r = shift @pubkey_requests;
  is($r->{verb}, 'GET', 'Pubkey request was a get');
  is($r->{uri}, '/pubkey', 'Public key was retrieved');

  $t->stop();
  $t->stop_daemons();
}

# No trusted-ca-certificates.crt is found by ESP, so no certificate checks.
$t->write_file('test.key', ApiManager::read_test_file('testdata/127.0.0.1.key'));
$t->write_file('test.crt', ApiManager::read_test_file('testdata/127.0.0.1.crt'));
success_test();

# Trusted-ca-certificates.crt is found, and the server's CA is in the list.
$t->write_file('trusted-ca-certificates.crt',
    ApiManager::read_test_file('testdata/127.0.0.1.crt'));
success_test();

# Trusted-ca-certificates.crt is found, but the server's CA is not in the list.
$t->write_file('trusted-ca-certificates.crt',
    ApiManager::read_test_file('testdata/roots.pem'));
$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&pubkey, $t, $PubkeyPort, Auth::get_public_key_jwk, 'pubkey.log');
$t->run_daemon(\&openid, $t, $OpenIdPort, 'openid.log');
is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');
is($t->waitforsocket("127.0.0.1:${OpenIdPort}"), 1, "openid socket (${OpenIdPort}) ready");
$t->run();

# Positive test case: Successfully completes the request via openID discovery.
my $response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
Authorization: Bearer $token

EOF


my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

like($response_headers, qr/HTTP\/1\.1 401 Unauthorized/,
     'Returned 401 Unauthorized.');

like($response_body,
     qr/JWT validation failed\: Unable to fetch URI of the key via OpenID discovery/,
     'OpenID discovery failed in response body');

my @openid_requests = ApiManager::read_http_stream($t, 'openid.log');
is(scalar @openid_requests, 0, 'OpenID was never called');

my @pubkey_requests = ApiManager::read_http_stream($t, 'pubkey.log');
is(scalar @pubkey_requests, 0, 'Pubkey was never called.');

$t->stop_daemons();
$t->stop();

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/shelves', <<'EOF');
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

sub openid {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file, 1)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/.well-known/openid-configuration', <<"EOF");
HTTP/1.1 200 OK
Connection: close

{
  "jwks_uri": "http://127.0.0.1:${PubkeyPort}/pubkey"
}
EOF

  $server->run();
}
