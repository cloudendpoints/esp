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

# TODO: refactor so nginx is only started once per test.
my $token = Auth::get_auth_token('./src/nginx/t/matching-client-secret.json');
my $pkey = Auth::get_public_key_jwk;

# --------------------------------------------------------------------
# Auth OK, cached key
# --------------------------------------------------------------------
$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&pubkey, $t, $PubkeyPort, $pkey, 'pubkey.log');
is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service Control socket ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');
$t->run();
make_request_n_validate_pubkey_fetch($t, 1, 'expect fetch');

# --------------------------------------------------------------------
# Auth OK, cached key (default 5 mins)
# --------------------------------------------------------------------
$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&pubkey, $t, $PubkeyPort, $pkey, 'pubkey.log');
is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service Control socket ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');
make_request_n_validate_pubkey_fetch($t, 0, 'expect no fetch');
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

{ "get": "ok" }
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
  my ($t, $port, $pkey, $file, $cache_control) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";

  local $SIG{PIPE} = 'IGNORE';

  my $response = <<'EOF';
HTTP/1.1 200 OK
Connection: close
EOF
  if ($cache_control) {
    $response .= "Cache-Control: $cache_control\r\n";
  }
  $response .= "\r\n$pkey";
  $server->on('GET', '/pubkey', $response);
  $server->run();
}

sub make_request_n_validate_pubkey_fetch {
  my ($t, $is_fetched, $comment) = @_;

  # Need to use different api-keys to avoid service_control cache.
  my $response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key-$is_fetched HTTP/1.0
Host: localhost
Authorization: Bearer $token

EOF
  my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

  like($response_headers, qr/HTTP\/1\.1 200 OK/, "Returned HTTP 200 ($comment).");
  is($response_body, <<'EOF', "Shelves returned in the response body ($comment).");
{ "get": "ok" }
EOF

  $t->stop_daemons();

  my @pubkey_requests = ApiManager::read_http_stream($t, 'pubkey.log');
  if ($is_fetched) {
    is(scalar @pubkey_requests, 1, 'Pubkey was fetched');

    my $r = shift @pubkey_requests;
    is($r->{verb}, 'GET', 'Pubkey request was a GET');
    is($r->{uri}, '/pubkey', 'Pubkey request path was a /pubkey');
    is($r->{headers}->{host}, "127.0.0.1:${PubkeyPort}", 'Host header was set');

  } else {
    is(scalar @pubkey_requests, 0, "No pubkey fetch (cached) ($comment).");
  }
}
