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
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(9);

# Save service name in the service configuration protocol buffer file.

$t->write_file('service.pb.txt', ApiManager::get_bookstore_service_config . <<"EOF");
backend {
  rules {
    selector: "ListShelves"
    address: "http://127.0.0.1:$BackendPort"
    path_translation: APPEND_PATH_TO_ADDRESS
  }
  rules {
    selector: "GetShelf"
    address: "http://127.0.0.1:$BackendPort/foo"
    path_translation: APPEND_PATH_TO_ADDRESS
  }
  rules {
    selector: "GetBook"
    address: "http://127.0.0.1:$BackendPort/"
    path_translation: APPEND_PATH_TO_ADDRESS
  }
}
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
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
        on;
      }
      proxy_pass \$backend_url;
    }
  }
}
EOF

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
$t->run();

################################################################################

# PathTranslation is set as APPEND_PATH_TO_ADDRESS.
my $response1 = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key');

# Backend address with a path prefix.
my $response123 = ApiManager::http_get($NginxPort,'/shelves/123?key=this-is-an-api-key&site=space%20plus%2B2U%3D');

# Backend address with an unexpected "/" sufix, should still work.
my $response2 = ApiManager::http_get($NginxPort,'/shelves/123/books/1234?key=this-is-an-api-key&timezone=EST');

# This one should fail since there is not backend rule specified for ListBooks
my $response3 = ApiManager::http_get($NginxPort,'/shelves/1/books?key=this-is-an-api-key');

$t->stop_daemons();

my ($response_headers1, $response_body1) = split /\r\n\r\n/, $response1, 2;

like($response_headers1, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body1, <<'EOF', 'Shelves returned in the response body.');
/shelves
EOF

my ($response_headers123, $response_body123) = split /\r\n\r\n/, $response123, 2;

like($response_headers123, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body123, <<'EOF', 'Shelves/111 returned in the response body.');
/foo/shelves/123
EOF

my ($response_headers2, $response_body2) = split /\r\n\r\n/, $response2, 2;

like($response_headers2, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body2, <<'EOF', 'Book returned in the response body.');
/books
EOF

my ($response_headers3, $response_body3) = split /\r\n\r\n/, $response3, 2;

like($response_headers3, qr/HTTP\/1\.1 500 Internal Server Error/, 'Returned HTTP 500.');

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/shelves?key=this-is-an-api-key', <<'EOF');
HTTP/1.1 200 OK
Connection: close

/shelves
EOF

  $server->on('GET', '/foo/shelves/123?key=this-is-an-api-key&site=space%20plus%2B2U%3D', <<'EOF');
HTTP/1.1 200 OK
Connection: close

/foo/shelves/123
EOF

  $server->on('GET', '/shelves/123/books/1234?key=this-is-an-api-key&timezone=EST', <<'EOF');
HTTP/1.1 200 OK
Connection: close

/books
EOF
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
Content-Type: application/json
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:report', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
  });

  $server->run();
}

################################################################################
