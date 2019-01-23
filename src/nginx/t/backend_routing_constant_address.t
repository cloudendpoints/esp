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
my $MetadataPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(14);

# Save service name in the service configuration protocol buffer file.

$t->write_file('service.pb.txt', ApiManager::get_bookstore_service_config_allow_all_http_requests . <<"EOF");
backend {
  rules {
    selector: "ListShelves"
    address: "http://127.0.0.1:$BackendPort/listShelves"
    path_translation: CONSTANT_ADDRESS
    jwt_audience: "test-audience"
  }
  rules {
    selector: "ListBooks"
    address: "http://127.0.0.1:$BackendPort/listBooks"
    path_translation: CONSTANT_ADDRESS
  }
  rules {
    selector: "GetBook"
    address: "http://127.0.0.1:$BackendPort/getBook"
    path_translation: CONSTANT_ADDRESS
    jwt_audience: "test-audience"
  }
}
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

$t->write_file('server_config.pb.txt', <<"EOF");
metadata_attributes {
  zone: "us-west1-d"
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
  endpoints {
    metadata_server http://127.0.0.1:${MetadataPort};
  }
  server {
    listen 127.0.0.1:${NginxPort};
    server_name localhost;
    location / {
      endpoints {
        api service.pb.txt;
        server_config server_config.pb.txt;
        on;
      }
      proxy_pass \$backend_url;
    }
  }
}
EOF

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&metadata, $t, $MetadataPort, 'metadata.log');
is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${MetadataPort}"), 1, 'Metadata socket ready.');

$t->run();

################################################################################

# PathTranslation is set as CONSTANT_ADDRESS. Authorization header is added
# from freshing token, with audience override.
my $response1 = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key');

# PathTranslation is set as CONSTANT_ADDRESS, with binding variables.
# no Authorization header is added.
my $response2 = ApiManager::http_get($NginxPort,'/shelves/123/books?key=this-is-an-api-key');

# PathTranslation is set as CONSTANT_ADDRESS, with binding variables and parameters.
# Authorization header is added from cached token, with audience override.
my $response3 = ApiManager::http_get($NginxPort,'/shelves/123/books/1234?key=this-is-an-api-key&timezone=EST');

$t->stop_daemons();

my ($response_headers1, $response_body1) = split /\r\n\r\n/, $response1, 2;
like($response_headers1, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body1, <<'EOF', 'Shelves returned in the response body.');
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

my ($response_headers2, $response_body2) = split /\r\n\r\n/, $response2, 2;
like($response_headers2, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body2, <<'EOF', 'Shelves returned in the response body.');
{ "books": [
    { "id": "1234", "titie": "Fiction" }
  ]
}
EOF

my ($response_headers3, $response_body3) = split /\r\n\r\n/, $response3, 2;
like($response_headers3, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body3, <<'EOF', 'Shelves returned in the response body.');
{ "id": "1234", "titie": "Fiction" }
EOF

# Check Authorization header is added into requests.
my @bookstore_requests = ApiManager::read_http_stream($t, 'bookstore.log');
is(scalar @bookstore_requests, 3, 'Bookstore received 3 requests.');

my $request = shift @bookstore_requests;
is($request->{headers}->{'authorization'}, 'Bearer test_audience_override',
    'Authorization header is received.' );

my $request = shift @bookstore_requests;
is($request->{headers}->{'authorization'}, undef);

my $request = shift @bookstore_requests;
is($request->{headers}->{'authorization'}, 'Bearer test_audience_override',
    'Authorization header is received.' );


# Check metadata server log.
my @metadata_requests = ApiManager::read_http_stream($t, 'metadata.log');
is(scalar @metadata_requests, 2, 'Metadata server received 2 request.');

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/listShelves', <<'EOF');
HTTP/1.1 200 OK
Connection: close

{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

  $server->on('GET', '/listBooks?shelf=123', <<'EOF');
HTTP/1.1 200 OK
Connection: close

{ "books": [
    { "id": "1234", "titie": "Fiction" }
  ]
}
EOF

  $server->on('GET', '/getBook?shelf=123&book=1234&timezone=EST', <<'EOF');
HTTP/1.1 200 OK
Connection: close

{ "id": "1234", "titie": "Fiction" }
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


sub metadata {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/computeMetadata/v1/instance/service-accounts/default/token', <<'EOF');
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

{
 "access_token":"ya29.7gFRTEGmovWacYDnQIpC9X9Qp8cH0sgQyWVrZaB1Eg1WoAhQMSG4L2rtaHk1",
 "expires_in":200,
 "token_type":"Bearer"
}
EOF

  $server->on('GET', '/computeMetadata/v1/instance/service-accounts/default/identity?audience=test-audience',  <<"EOF");
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

test_audience_override\r\n\r\n
EOF

  $server->run();
}

################################################################################
