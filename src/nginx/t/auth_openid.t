# Copyright (C) Endpoints Server Proxy Authors
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

BEGIN { use FindBin; chdir($FindBin::Bin); }

use ApiManager;   # Must be first (sets up import path to the Nginx test module)
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework
use HttpServer;
use Auth;

################################################################################

# Test openID discovery.

# Port assignments
my $NginxPort = 8080;
my $BackendPort = 8081;
my $ServiceControlPort = 8082;
my $PubkeyPort = 8083;
my $OpenIdPort = 8085;
my $OpenIdPortNoexist = 8087;
my $OpenIdPortBadContent = 8089;

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(20);

my $config = ApiManager::get_bookstore_service_config;
$config .= <<"EOF";
authentication {
  providers: [
   {
     id: "test_auth"
     issuer: "http://127.0.0.1:${OpenIdPort}"
   },
   {
     id: "openid_nonexist"
     issuer: "http://127.0.0.1:${OpenIdPortNoexist}"
   },
   {
     id: "openid_bad_content"
     issuer: "http://127.0.0.1:${OpenIdPortBadContent}"
   }
  ],
  rules: {
    selector: "ListShelves"
    requirements: [
     {
       provider_id: "test_auth"
     },
     {
       provider_id: "openid_nonexist"
     },
     {
       provider_id: "openid_bad_content"
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
my $token = 'eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6ImIzMzE5YTE0NzUxNGRmN2VlNWU0YmNkZWU1MTM1MGNjODkwY2M4OWUifQ.eyJpc3MiOiJodHRwOi8vMTI3LjAuMC4xOjgwODUiLCJzdWIiOiJlbmQtdXNlci1pZCIsImF1ZCI6ImVuZHBvaW50cy10ZXN0LmNsb3VkZW5kcG9pbnRzYXBpcy5jb20iLCJpYXQiOjE0NjE3NzkzMjEsImV4cCI6MjQ2MTc4MjkyMX0.eDmjL70VVP8lCH-m_0Yaqv-cofGhHx5Ph0Rj0RN9Y6gLFqqJOW02AIIAMYDvahPHg7bOQE1ojCJsGO3HrElgeYMHRaqPnk2HFIlVUuy6-ECNQXX1T3dAq6dw_zjp8xIsC_od04054qGDoay2r3fCRrcCOFYTmyU53WLuwzviuiIenyr5qW1z1nA9uzSBmR8qQLLvKgCCO9TPrcEt8dIM6ea8dybDBkx6wkYAnLXxN9xXcadLEQgzwce77kIZgUGDUbrO01SNnAWF6NigPPK25U_33LDvoeu9WF1XVYBM87RSY9va78EdO71jQh38iUzBgGeUsYUCRbC254MfBl-IVA';

# Token generated with the following header and payload, using private key from matching-client-secret.json.
# Header:
# {
#   "alg": "RS256",
#   "typ": "JWT",
#   "kid": "b3319a147514df7ee5e4bcdee51350cc890cc89e"
# }
# Payload:
# {
#   "iss": "http://127.0.0.1:${OpenIdPortNoexist}",
#   "sub": "user-id-openid-nonexist",
#   "aud": "endpoints-test.cloudendpointsapis.com",
#   "iat": 1461779321,
#   "exp": 2461782921
# }
my $token_no_openid = 'eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6ImIzMzE5YTE0NzUxNGRmN2VlNWU0YmNkZWU1MTM1MGNjODkwY2M4OWUifQ.eyJpc3MiOiJodHRwOi8vMTI3LjAuMC4xOjgwODciLCJzdWIiOiJ1c2VyLWlkLW9wZW5pZC1ub25leGlzdCIsImF1ZCI6ImVuZHBvaW50cy10ZXN0LmNsb3VkZW5kcG9pbnRzYXBpcy5jb20iLCJpYXQiOjE0NjE3NzkzMjEsImV4cCI6MjQ2MTc4MjkyMX0.FQ2JYyUeQoRoFxf062X2egKuLAAdDdsGvWhlvds-LhezI6-DDLk3xdP3jAQkUSxBmA29fmDRMCdfghEr0Irq8hWpaL_YRZRQbR7vvbEvZsHpWeYCFpZk6vvZW8kgVYp9V6cEicGkJ7AvTxdBfbprjawHIjx188NRQ7hJj6Rm23zsZLWANNPGkkLPopT-X9wGFpACy8StR-HRcV34O_eoqStIJdgNjRGNJm-Pzkh3QSav5fZliAemHqGjNiPnoGZZrqCZQ_6r7yw2q9Ju-nZxQ-X_rlQSC00M0nVxOQIvdOWjzK-f5yyTH7kanY2s-I8Zxbr-iDdm7TbCSOlaPw_OKg';

# Token generated with the following header and payload, using private key from matching-client-secret.json.
# Header:
# {
#   "alg": "RS256",
#   "typ": "JWT",
#   "kid": "b3319a147514df7ee5e4bcdee51350cc890cc89e"
# }
# Payload:
# {
#   "iss": "http://127.0.0.1:${OpenIdPortBadContent}",
#   "sub": "user-id-openid-bad-content",
#   "aud": "endpoints-test.cloudendpointsapis.com",
#   "iat": 1461779321,
#   "exp": 2461782921
# }
my $token_openid_bad_content = 'eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6ImIzMzE5YTE0NzUxNGRmN2VlNWU0YmNkZWU1MTM1MGNjODkwY2M4OWUifQ.eyJpc3MiOiJodHRwOi8vMTI3LjAuMC4xOjgwODkiLCJzdWIiOiJ1c2VyLWlkLW9wZW5pZC1iYWQtY29udGVudCIsImF1ZCI6ImVuZHBvaW50cy10ZXN0LmNsb3VkZW5kcG9pbnRzYXBpcy5jb20iLCJpYXQiOjE0NjE3NzkzMjEsImV4cCI6MjQ2MTc4MjkyMX0.OKyA4dewOM8b-0FadPuCFuoqc8Cht2meZS1eXjwoHoKMkR7OIhgdvGfM3SBxpF1v0Hi6RxOKpreXmjrRaFCMJMgJD6LxTL0ZWnkAmb6fEKiLZrqpYXm2Mu74FglvpOTxAi3fYkiUB3kAQiYbP7qN-8-SxHjwsXYbgYYEvgZqBaDbUZVsDCQDqJxZZY0U7denFf99bCDBovG1kcPKG9umfIDtV8soge-7ffWpgWb9okygQ0M1YiWEyrGSMaHDXz6oVV-awuRNMz0XhLiiQZbAEo5jxXY-yFoQQJJ1OCnu0Ni7qpQlgL5bgLCeWFnOTTNUttWWYP0AQS5eg-RwJZcsJA';

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&pubkey, $t, $PubkeyPort, Auth::get_public_key_jwk, 'pubkey.log');
$t->run_daemon(\&openid, $t, $OpenIdPort, 'openid.log');
$t->run_daemon(\&openid_bad, $t, $OpenIdPortBadContent, 'openid-bad.log');
is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');
is($t->waitforsocket("127.0.0.1:${OpenIdPort}"), 1, "openid socket (${OpenIdPort}) ready");
is($t->waitforsocket("127.0.0.1:${OpenIdPortBadContent}"), 1, "openid socket (${OpenIdPortBadContent}) ready");
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

# Negative test case: openID discovery link is invalid.
$response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
Authorization: Bearer $token_no_openid

EOF

($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

like($response_headers, qr/HTTP\/1\.1 401 Unauthorized/,
     'Returned 401 Unauthorized.');

like($response_body,
     qr/JWT validation failed\: Unable to fetch URI of the key via OpenID discovery/,
     'OpenID discovery failed in response body');

# Negative test case: openID discovery doc does not contain "jwks_uri" field.
$response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
Authorization: Bearer $token_openid_bad_content

EOF

($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

like($response_headers, qr/HTTP\/1\.1 401 Unauthorized/,
     'Returned 401 Unauthorized.');

like($response_body,
     qr/JWT validation failed\: Unable to parse URI of the key via OpenID discovery/,
     'OpenID discovery failed in response body');

my @openid_bad_requests = ApiManager::read_http_stream($t, 'openid-bad.log');
is(scalar @openid_bad_requests, 1, 'bad openid discovery was called once');
$r = shift @openid_bad_requests;
is($r->{verb}, 'GET', 'bad openid discovery request was a get');
is($r->{uri}, '/.well-known/openid-configuration',
   'OpenID discovery doc was retrieved');

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
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
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

# The openid discovery site that does not contain "jwks_uri".
sub openid_bad {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/.well-known/openid-configuration', <<"EOF");
HTTP/1.1 200 OK
Connection: close

{
  "issuer": "http://127.0.0.1:${OpenIdPortBadContent}"
}
EOF

  $server->run();
}
