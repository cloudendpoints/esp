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
use src::nginx::t::ServiceControl;
use src::nginx::t::Auth;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework

################################################################################

# Port allocations

my $NginxPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(5);

my $config = ApiManager::get_bookstore_service_config .<<"EOF";
producer_project_id: "endpoints-test"
authentication {
  providers {
    id: "test_auth"
    issuer: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l\@developer.gserviceaccount.com"
    jwks_uri: "http://127.0.0.1/pubkey"
    authorization_url: "http://dummy-redirect-url"
  }
  rules {
    selector: "ListShelves"
    requirements {
      provider_id: "test_auth"
      audiences: "ok_audience_1"
    }
  }
}
control {
  environment: "http://127.0.0.1:3000"
}
EOF
$t->write_file('service.pb.txt', $config);

# enable the redirect_authorization_url flag
ApiManager::write_file_expand($t, 'server_config.txt', <<"EOF");
api_authentication_config {
  redirect_authorization_url: true
}
EOF

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
        server_config server_config.txt;
        on;
      }
      proxy_pass http://127.0.0.1:3000;
    }
  }
}
EOF

$t->run();

################################################################################
# no auth token
my $response1 = ApiManager::http_get($NginxPort, "/shelves");

# should redirect
like($response1, qr/HTTP\/1\.1 302 Moved Temporarily/, 'Returned HTTP 302.');
like($response1, qr/Location: http:\/\/dummy-redirect-url/, 'Return correct redirect location.');

# expired auth token
my $expired_token = Auth::get_expired_jwt_token;
my $response2 = ApiManager::http($NginxPort, <<"EOF");
GET /shelves HTTP/1.0
Host: localhost
Authorization: Bearer $expired_token

EOF

# should redirect
like($response2, qr/HTTP\/1\.1 302 Moved Temporarily/, 'Returned HTTP 302.');
like($response2, qr/Location: http:\/\/dummy-redirect-url/, 'Return correct redirect location.');

#invalid auth token
my $response3 = ApiManager::http($NginxPort, <<"EOF");
GET /shelves HTTP/1.0
Host: localhost
Authorization: Bearer invalid_token

EOF

# should not redirect.
like($response3, qr/HTTP\/1\.1 401 Unauthorized/, 'Returned HTTP 401.');

$t->stop_daemons();

################################################################################

