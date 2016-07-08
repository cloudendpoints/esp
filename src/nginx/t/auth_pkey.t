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

# Port assignments
my $NginxPort = 8080;
my $BackendPort = 8081;
my $ServiceControlPort = 8082;
my $PubkeyPort = 8083;

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(50 * 2);

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

my $pkey_jwk = Auth::get_public_key_jwk;
my $pkey_x509 = Auth::get_public_key_x509;

# TODO: refactor so nginx is only started once per test.
foreach my $pkey($pkey_jwk, $pkey_x509) {
  $t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
  $t->run_daemon(\&servicecontrol, $t, $ServiceControlPort,'servicecontrol.log');
  $t->run_daemon(\&pubkey, $t, $PubkeyPort, $pkey, 'pubkey.log');
  is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
  is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
  is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');
  $t->run();

  ################################################################################

  # Missing credentials.
  my $response = http_get('/shelves');
  like($response, qr/HTTP\/1\.1 401 Unauthorized/, 'Returned HTTP 401, missing creds.');
  like($response, qr/WWW-Authenticate: Bearer/, 'Returned auth challenge.');
  like($response, qr/Content-Type: application\/json/i,
       'Missing creds returned application/json body.');
  like($response, qr/JWT validation failed: Missing or invalid credentials/i,
       "Error body contains 'Missing or invalid credentials'.");

  # Invalid credentials.
  $response = http(<<'EOF');
GET /shelves HTTP/1.0
Host: localhost
Authorization: Bearer invalid.token

EOF
  like($response, qr/HTTP\/1\.1 401 Unauthorized/, 'Returned HTTP 401, invalid token.');
  like($response, qr/WWW-Authenticate: Bearer, error=\"invalid_token\"/, 'Returned invalid_token challenge.');
  like($response, qr/Content-Type: application\/json/i,
       'Invalid token returned application/json body.');
  like($response, qr/JWT validation failed: BAD_FORMAT/i, "Error body contains 'invalid token'.");

  # Token generated from different issuer/key.
  my $token = Auth::get_auth_token('./wrong-client-secret.json');
  $response = http(<<"EOF");
GET /shelves HTTP/1.0
Host: localhost
Authorization: Bearer $token

EOF
  like($response, qr/HTTP\/1\.1 401 Unauthorized/, 'Returned HTTP 401, no matching pkey.');
  like($response, qr/WWW-Authenticate: Bearer, error=\"invalid_token\"/, 'Returned invalid_token challenge.');
  like($response, qr/Content-Type: application\/json/i,
       'No matching pkey returned application/json body.');
  like($response, qr/JWT validation failed: Issuer not allowed/i, "Error body contains 'issuer not allowed'.");


  # Audience not allowed.
  $token = Auth::get_auth_token('./matching-client-secret.json', 'bad_audience');
  $response = http(<<"EOF");
GET /shelves HTTP/1.0
Host: localhost
Authorization: Bearer $token

EOF
  like($response, qr/HTTP\/1\.1 403 Forbidden/, 'Returned HTTP 403, audience not allowed.');
  like($response, qr/WWW-Authenticate: Bearer, error=\"invalid_token\"/, 'Returned invalid_token challenge.');
  like($response, qr/Content-Type: application\/json/i,
       'No matching pkey returned application/json body.');
  like($response, qr/JWT validation failed: Audience not allowed/i, "Error body contains 'audience not allowed'.");

  $t->stop_daemons();
  my $no_bookstore_requests = $t->read_file('bookstore.log');
  is($no_bookstore_requests, '', 'Request did not reach the backend.');
  my $no_pubkey_requests = $t->read_file('pubkey.log');
  is($no_pubkey_requests, '', 'No pubkey fetch (bad token).');

  # Key is unreachable.
  $token = Auth::get_auth_token('./matching-client-secret.json', 'ok_audience_1');
  $response = http(<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
Authorization: Bearer $token

EOF
  like($response, qr/HTTP\/1\.1 401 Unauthorized/, 'Returned HTTP 401, unable to fetch key.');
  like($response, qr/WWW-Authenticate: Bearer, error=\"invalid_token\"/, 'Returned invalid_token challenge.');
  like($response, qr/Content-Type: application\/json/i,
       'Unable to fetch key returned application/json body.');

  # Auth OK with allowed audience.
  $t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
  $t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
  $t->run_daemon(\&pubkey, $t, $PubkeyPort, $pkey, 'pubkey.log');
  is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
  is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
  is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');

  $token = Auth::get_auth_token('./matching-client-secret.json', 'ok_audience_1');
  # OK requests need to use different api-keys to avoid service_control cache.
  $response = http(<<"EOF");
GET /shelves?key=this-is-an-api-key1 HTTP/1.0
Host: localhost
Authorization: Bearer $token

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
  is($r->{verb}, 'GET', 'Backend received get');
  is($r->{uri}, '/shelves?key=this-is-an-api-key1', 'Backend received get /shelves w/ key');
  is($r->{headers}->{host}, "127.0.0.1:${BackendPort}", 'Host header was set.');
  isnt($r->{headers}->{authorization}, undef, 'Authorization header was received');

  my @pubkey_requests = ApiManager::read_http_stream($t, 'pubkey.log');
  is(scalar @pubkey_requests, 1, 'Pubkey received one request');
  $r = shift @pubkey_requests;
  is($r->{verb}, 'GET', 'Pubkey request was a get');
  is($r->{uri}, '/pubkey', 'Pubkey received GET /pubkey');
  is($r->{headers}->{host}, "127.0.0.1:${PubkeyPort}", 'Host header was set');

  # Auth OK with audience = service_name and token in url parameter.
  $t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
  $t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
  $t->run_daemon(\&pubkey, $t, $PubkeyPort, $pkey, 'pubkey.log');
  is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
  is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
  is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');

  $token = Auth::get_auth_token('./matching-client-secret.json');
  # OK requests need to use different api-keys to avoid service_control cache.
  $response = http(<<"EOF");
GET /shelves?key=this-is-an-api-key2&access_token=$token HTTP/1.0
Host: localhost

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

  @bookstore_requests = ApiManager::read_http_stream($t, 'bookstore.log');
  is(scalar @bookstore_requests, 1, 'Backend received one request.');
  $r = shift @bookstore_requests;
  is($r->{verb}, 'GET', 'Backend request was a get');
  like($r->{uri}, qr/\/shelves\?key=this-is-an-api-key2&access_token=\S+/, 'Access token in query parameter.');
  is($r->{headers}->{host}, "127.0.0.1:${BackendPort}", 'Host header was set');

  $no_pubkey_requests = $t->read_file('pubkey.log');
  is($no_pubkey_requests, '', 'No pubkey fetch (cached).');

  $t->stop();
}  # End foreach $pkey.

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
