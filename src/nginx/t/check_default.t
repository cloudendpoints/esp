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
use ServiceControl;
use JSON::PP;

################################################################################

# Port assignments
my $NginxPort = 8080;
my $BackendPort = 8081;
my $ServiceControlPort = 8082;

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(37);

# Save service name in the service configuration protocol buffer file.
$t->write_file('service.pb.txt', <<"EOF");
name: "endpoints-test.cloudendpointsapis.com"
http {
  rules {
    selector: "google.serviceruntime.Unknown.Delete"
    delete: "/**"
  }
  rules {
    selector: "google.serviceruntime.Unknown.Get"
    get: "/**"
  }
  rules {
    selector: "google.serviceruntime.Unknown.Patch"
    patch: "/**"
  }
  rules {
    selector: "google.serviceruntime.Unknown.Post"
    post: "/**"
  }
  rules {
    selector: "google.serviceruntime.Unknown.Put"
    put: "/**"
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

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');

$t->run();

################################################################################

my @tests = (
  { verb => 'GET', url => '/shelves?key=this-is-an-api-key' },
  { verb => 'DELETE', url => '/shelves/1?key=this-is-an-api-key' },
  { verb => 'POST', url => '/shelves?key=this-is-an-api-key', body => '{ "verb": "POST" }' },
  { verb => 'PUT', url => '/shelves/2?key=this-is-an-api-key', body => '{ "verb": "PUT" }' },
  { verb => 'PATCH', url => '/shelves/3?key=this-is-an-api-key', body => '{ "verb": "PATCH" }' },
);

for my $test ( @tests ) {
  my $response = http(<<"EOF", body => $test->{body});
$test->{verb} $test->{url} HTTP/1.0
Host:localhost

EOF
  my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;
  like($response_headers, qr/HTTP\/1\.1 200 OK/, "$test->{verb} Returned HTTP 200.");
}

$t->stop_daemons();

my $r;

my @bookstore_requests = ApiManager::read_http_stream($t, 'bookstore.log');
is(scalar @bookstore_requests, 5, 'Bookstore received 5 requests.');

$r = shift @bookstore_requests;
is($r->{verb}, 'GET', 'get request');
is($r->{path}, '/shelves', 'get /shelves was called');
is($r->{headers}->{host}, "127.0.0.1:${BackendPort}", 'request 1 host header');

$r = shift @bookstore_requests;
is($r->{verb}, 'DELETE', 'delete request');
is($r->{path}, '/shelves/1', 'delete /shelves/1 was called');

$r = shift @bookstore_requests;
is($r->{verb}, 'POST', 'post request');
is($r->{path}, '/shelves', 'post /shelves was called');

$r = shift @bookstore_requests;
is($r->{verb}, 'PUT', 'put request');
is($r->{path}, '/shelves/2', 'put /shelves/2 was called');

$r = shift @bookstore_requests;
is($r->{verb}, 'PATCH', 'patch request');
is($r->{path}, '/shelves/3', 'patch /shelves/3 was called');
is($r->{uri}, '/shelves/3?key=this-is-an-api-key', 'path api key included');

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 5, 'Service control received 5 requests.');

my $check;

# Request 1
$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', 'Request 1 was POST');
is($r->{path}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', 'Request 1 path was :check');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', 'content-type is protouf');

$check = decode_json(ServiceControl::convert_proto($r->{body}, 'check_request', 'json'));
is($check->{operation}->{operationName}, 'google.serviceruntime.Unknown.Get', 'unknown get matched');

# Request 2
$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', 'Request 2 was POST');
is($r->{path}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', 'Request 2 path was :check');

$check = decode_json(ServiceControl::convert_proto($r->{body}, 'check_request', 'json'));
is($check->{operation}->{operationName}, 'google.serviceruntime.Unknown.Delete', 'unknown delete matched');

$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', 'Request 3 was POST');
is($r->{path}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', 'Request 3 path was :check');

$check = decode_json(ServiceControl::convert_proto($r->{body}, 'check_request', 'json'));
is($check->{operation}->{operationName}, 'google.serviceruntime.Unknown.Post', 'unknown post matched');

$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', 'Request 4 was POST');
is($r->{path}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', 'Request 4 path was :check');

$check = decode_json(ServiceControl::convert_proto($r->{body}, 'check_request', 'json'));
is($check->{operation}->{operationName}, 'google.serviceruntime.Unknown.Put', 'unknown put matched');

$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', 'Request 5 was POST');
is($r->{path}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', 'Request 5 path was :check');

$check = decode_json(ServiceControl::convert_proto($r->{body}, 'check_request', 'json'));
is($check->{operation}->{operationName}, 'google.serviceruntime.Unknown.Patch', 'unknown path matched');

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/shelves?key=this-is-an-api-key', <<'EOF');
HTTP/1.1 200 OK
Connection: close

{ "get": "ok" }
EOF

  $server->on('DELETE', '/shelves/1?key=this-is-an-api-key', <<'EOF');
HTTP/1.1 200 OK
Connection: close

EOF

  $server->on('POST', '/shelves?key=this-is-an-api-key', <<'EOF');
HTTP/1.1 200 OK
Connection: close

{ "post": "ok" }
EOF

  $server->on('PUT', '/shelves/2?key=this-is-an-api-key', <<'EOF');
HTTP/1.1 200 OK
Connection: close

{ "put": "ok" }
EOF

  $server->on('PATCH', '/shelves/3?key=this-is-an-api-key', <<'EOF');
HTTP/1.1 200 OK
Connection: close

{ "patch": "ok" }
EOF

  $server->run();
}

sub servicecontrol {
  my ($t, $port, $file) = @_;

  # Save requests (last argument).
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file, 1)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  # Expecting 5 calls to :check
  for (my $i = 0; $i < 5; $i++) {
    $server->on('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', <<'EOF');
HTTP/1.1 200 OK
Connection: close

EOF
}

  $server->run();
}

################################################################################
