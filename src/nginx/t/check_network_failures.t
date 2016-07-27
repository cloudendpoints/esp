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

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(13);

ApiManager::write_file_expand($t, 'sc_timeout.pb.txt', <<"EOF");
service_control_config {
  check_timeout_ms: 1000
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
        server_config sc_timeout.pb.txt;
        on;
      }
      proxy_pass http://127.0.0.1:${BackendPort};
    }
  }
}
EOF

################################################################################
#
#  Failure with wrong service_control server name
#  Only starts Nginx
#
################################################################################

# Wrong service control server domain name
my $config1 = ApiManager::get_bookstore_service_config . <<"EOF";
control {
  environment: "http://wrong_service_control_server_name"
}
EOF

$t->write_file('service.pb.txt', $config1);

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
$t->run();

my $response1 = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key');

$t->stop();
$t->stop_daemons();

like($response1, qr/HTTP\/1\.1 503 Service Temporarily Unavailable/, 'Returned HTTP 503.');
like($response1, qr/Failed to connect to service control/i, 'Failed to connect to service control.');

my $bookstore_requests1 = $t->read_file('bookstore.log');
is($bookstore_requests1, '', 'Request did not reach the backend.');

################################################################################
#
#  Failure with service_control server not running.
#  Only starts Nginx
#
################################################################################

# service control server is not running.
my $config2 = ApiManager::get_bookstore_service_config . <<"EOF";
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

$t->write_file('service.pb.txt', $config2);

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
$t->run();

my $response2 = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key');

$t->stop();
$t->stop_daemons();

like($response2, qr/HTTP\/1\.1 503 Service Temporarily Unavailable/, 'Returned HTTP 503.');
like($response2, qr/Failed to connect to service control/i, 'Failed to connect to service control.');

my $bookstore_requests2 = $t->read_file('bookstore.log');
is($bookstore_requests2, '', 'Request did not reach the backend.');

################################################################################
#
#  Failure with service_control timeout on response.
#  starts Nginx, service_control and backend.
#
################################################################################

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
$t->run();

my $response3 = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key');

$t->stop();
$t->stop_daemons();

like($response3, qr/HTTP\/1\.1 503 Service Temporarily Unavailable/, 'Returned HTTP 503.');
like($response3, qr/Failed to connect to service control/i, 'Failed to connect to service control.');

my $bookstore_requests3 = $t->read_file('bookstore.log');
is($bookstore_requests3, '', 'Request did not reach the backend.');

################################################################################

sub servicecontrol {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;

    # sleep 2 seconds to cause time-out.
    sleep 2;

    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->run();
}

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  # Do not initialize any server state, requests won't reach backend anyway.

  $server->run();
}

################################################################################
