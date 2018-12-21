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

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(48);

my $config = ApiManager::get_bookstore_service_config;
$config .= <<"EOF";
authentication {
  providers {
    id: "test_auth"
    issuer: "test-esp-auth.com"
    jwks_uri: "http://127.0.0.1:${PubkeyPort}/key"
  }
  rules {
    selector: "ListShelves"
    requirements {
      provider_id: "test_auth"
      audiences: "ok_audience_1,ok_audience_2"
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

# From matching-client-secret-symmetric.json
my $key = "abcedfgabcdefg";

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&key, $t, $PubkeyPort, $key, 'key.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');

$t->run();

################################################################################

# Missing credentials.
my $response = ApiManager::http_get($NginxPort,'/shelves');
like($response, qr/HTTP\/1\.1 401 Unauthorized/, 'Returned HTTP 401, missing creds.');
like($response, qr/WWW-Authenticate: Bearer/, 'Returned auth challenge.');
like($response, qr/Content-Type: application\/json/i,
     'Missing creds returned application/json body.');
like($response, qr/JWT validation failed: Missing or invalid credentials/i,
     "Error body contains 'Missing or invalid credentials'.");

# Invalid credentials.
$response = ApiManager::http($NginxPort,<<'EOF');
GET /shelves HTTP/1.0
Host: localhost
Authorization: Bearer invalid.token

EOF
like($response, qr/HTTP\/1\.1 401 Unauthorized/, 'Returned HTTP 401, invalid token.');
like($response, qr/WWW-Authenticate: Bearer, error=\"invalid_token\"/, 'Returned invalid_token challenge.');
like($response, qr/Content-Type: application\/json/i,
     'Invalid token returned application/json body.');
like($response, qr/JWT validation failed: BAD_FORMAT/i,
     "Error body contains 'bad format'.");

# Token generated from different issuer/key.
my $token = Auth::get_auth_token('./src/nginx/t/wrong-client-secret-symmetric.json');
$response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves HTTP/1.0
Host: localhost
Authorization: Bearer $token

EOF
like($response, qr/HTTP\/1\.1 401 Unauthorized/, 'Returned HTTP 401, no matching client secret.');
like($response, qr/WWW-Authenticate: Bearer, error=\"invalid_token\"/, 'Returned invalid_token challenge.');
like($response, qr/Content-Type: application\/json/i,
     'No matching client secret returned application/json body.');


# Audience not allowed.
$token = Auth::get_auth_token('./src/nginx/t/matching-client-secret-symmetric.json', 'bad_audience');
$response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves HTTP/1.0
Host: localhost
Authorization: Bearer $token

EOF
like($response, qr/HTTP\/1\.1 403 Forbidden/, 'Returned HTTP 403, audience not allowed.');
like($response, qr/WWW-Authenticate: Bearer, error=\"invalid_token\"/, 'Returned invalid_token challenge.');
like($response, qr/Content-Type: application\/json/i,
     'Audience not allowed returned application/json body.');

$t->stop_daemons();
my $no_bookstore_requests = $t->read_file('bookstore.log');
is($no_bookstore_requests, '', 'Request did not reach the backend.');
my $no_key_requests = $t->read_file('key.log');
is($no_key_requests, '', 'No client secret fetch (bad token).');

# Key is unreachable.
$token = Auth::get_auth_token('./src/nginx/t/matching-client-secret-symmetric.json', 'ok_audience_1');
$response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
Authorization: Bearer $token

EOF
like($response, qr/HTTP\/1\.1 401 Unauthorized/, 'Returned HTTP 401, unable to fetch key.');
like($response, qr/WWW-Authenticate: Bearer, error=\"invalid_token\"/, 'Returned invalid_token challenge.');
like($response, qr/Content-Type: application\/json/i,
     'Unable to fetch key returned application/json body.');


# Auth OK with allowed audience, key is not cached, token in url parameter.
$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&key, $t, $PubkeyPort, $key, 'key.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control port ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');

$token = Auth::get_auth_token('./src/nginx/t/matching-client-secret-symmetric.json', 'ok_audience_1');
# OK requests need to use different api-keys to avoid service_control cache.
$response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key1&access_token=$token HTTP/1.0
Host: localhost

EOF
my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
unlike($response_headers, qr/WWW-Authenticate/, 'Returned auth challenge.');
is($response_body, <<'EOF', 'Shelves returned in the response body.');
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

$t->stop_daemons();

my @bookstore_requests = ApiManager::read_http_stream($t, 'bookstore.log');
is(scalar @bookstore_requests, 1, 'Backend received one request');
my $r = shift @bookstore_requests;
is($r->{verb}, 'GET', 'Backend request was a get');
like($r->{uri}, qr/\/shelves\?key=this-is-an-api-key1&access_token=\S+/, 'Backend request was get /shelves');
is($r->{headers}->{host}, "127.0.0.1:${BackendPort}", 'Host header was set.');

my @key_requests = ApiManager::read_http_stream($t, 'key.log');
is(scalar @key_requests, 1, 'There was one key request');
$r = shift @key_requests;
is($r->{verb}, 'GET', 'Key request was a get');
is($r->{uri}, '/key', 'Key uri was /key');
is($r->{headers}->{host}, "127.0.0.1:${PubkeyPort}", 'Host header was set');

# Auth OK with audience = service_name.
$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&key, $t, $PubkeyPort, $key, 'key.log');
is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control port ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');

$token = Auth::get_auth_token('./src/nginx/t/matching-client-secret-symmetric.json');
# OK requests need to use different api-keys to avoid service_control cache.
$response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key2 HTTP/1.0
Host: localhost
Authorization: Bearer $token

EOF
($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
unlike($response_headers, qr/WWW-Authenticate/, 'Returned auth challenge.');
is($response_body, <<'EOF', 'Shelves returned in the response body.');
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

$t->stop_daemons();

my @bookstore_requests = ApiManager::read_http_stream($t, 'bookstore.log');
is(scalar @bookstore_requests, 1, 'Backend received one request');
my $r = shift @bookstore_requests;
is($r->{verb}, 'GET', 'Backend request was a get');
like($r->{uri}, qr/\/shelves\?key=this-is-an-api-key2$/, 'Backend request was get /shelves');
is($r->{headers}->{host}, "127.0.0.1:${BackendPort}", 'Host header was set.');
like($r->{headers}->{authorization}, qr/Bearer \S+/, 'Backend was was authenticated.');

$no_key_requests = $t->read_file('key.log');
is($no_key_requests, '', 'No key fetch (cached).');

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

sub key {
  my ($t, $port, $secret, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/key', <<"EOF");
HTTP/1.1 200 OK
Connection: close

$secret
EOF

  $server->run();
}
