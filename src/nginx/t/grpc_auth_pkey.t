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
my $ServiceControlPort = ApiManager::pick_port();
my $Http2NginxPort = ApiManager::pick_port();
my $GrpcBackendPort = ApiManager::pick_port();
my $PubkeyPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(5);

$t->write_file('service.pb.txt', ApiManager::get_grpc_test_service_config($GrpcBackendPort) . <<"EOF");
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
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

ApiManager::write_file_expand($t, 'nginx.conf', <<"EOF");
%%TEST_GLOBALS%%
daemon off;
events {
  worker_connections 32;
}
http {
  %%TEST_GLOBALS_HTTP%%
  server {
    listen 127.0.0.1:${Http2NginxPort} http2;
    server_name localhost;
    location / {
      endpoints {
        api service.pb.txt;
        %%TEST_CONFIG%%
        on;
      }
      grpc_pass;
    }
  }
}
EOF

my $pkey_jwk = Auth::get_public_key_jwk;
my $auth_token = Auth::get_auth_token('./src/nginx/t/matching-client-secret.json', 'ok_audience_1');

$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'requests.log');
$t->run_daemon(\&pubkey, $t, $PubkeyPort, $pkey_jwk, 'pubkey.log');
$t->run_daemon(\&ApiManager::grpc_test_server, $t, "127.0.0.1:${GrpcBackendPort}");
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');
is($t->waitforsocket("127.0.0.1:${GrpcBackendPort}"), 1, 'GRPC test server socket ready.');
$t->run();
is($t->waitforsocket("127.0.0.1:${Http2NginxPort}"), 1, 'Nginx socket ready.');

################################################################################

my $test_results = &ApiManager::run_grpc_test($t, <<"EOF");
server_addr: "127.0.0.1:${Http2NginxPort}"
plans {
  echo {
    call_config {
      api_key: "this-is-an-api-key"
    }
    request {
      text: "Hello, world!"
    }
  }
}
plans {
  echo {
    call_config {
      api_key: "this-is-an-api-key"
      auth_token: "invalid.token"
    }
    request {
      text: "Hello, world!"
    }
  }
}
plans {
  echo {
    call_config {
      api_key: "this-is-an-api-key"
      auth_token: "${auth_token}"
    }
    request {
      text: "Hello, world!"
    }
    expected_metadata_keys: ["x-endpoint-api-userinfo"]
  }
}
EOF

$t->stop_daemons();

my $test_results_expected = <<'EOF';
results {
  status {
    code: 16
    details: "JWT validation failed: Missing or invalid credentials"
  }
}
results {
  status {
    code: 16
    details: "JWT validation failed: Bad JWT format: Invalid JSON in header"
  }
}
results {
  echo {
    text: "Hello, world!"
  }
  additional_metadata {
    key: "x-endpoint-api-userinfo"
    value: "eyJ(\S)+"
  }
}
EOF
like($test_results, qr/$test_results_expected/m, 'Client tests completed as expected.');
################################################################################

sub service_control {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";

  $server->on('POST', '/v1/services/endpoints-grpc-test.cloudendpointsapis.com:check', <<'EOF');
HTTP/1.1 200 OK
Connection: close

EOF

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
