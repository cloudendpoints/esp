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
my $NoopPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(8);
$t->write_file('service.pb.txt', ApiManager::get_bookstore_service_config . <<"EOF");
control {
  environment: "http://127.0.0.1:${NoopPort}"
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
    location /status {
      endpoints_status;
    }
    location / {
      endpoints {
        api service.pb.txt;
        %%TEST_CONFIG%%
        on;
      }
      proxy_pass http://127.0.0.1:${NoopPort};
    }
  }
}
EOF

$t->run_daemon(\&http_noop_server, $t, $NoopPort, 'requests.log');
is($t->waitforsocket("127.0.0.1:${NoopPort}"), 1, 'Noop server socket ready.');

$t->run();

################################################################################

my $response = ApiManager::http_get($NginxPort,'/status');
$t->stop_daemons();

like($response, qr/"totalCalledChecks": "0"/, 'Returned total called checks.');
like($response, qr/"sendChecksByFlush": "0"/, 'Returned checks by flush.');
like($response, qr/"sendChecksInFlight": "0"/, 'Returned checks in flight.');
like($response, qr/"totalCalledReports": "0"/, 'Returned total called reports.');
like($response, qr/"sendReportsByFlush": "0"/, 'Returned reports by flush.');
like($response, qr/"sendReportsInFlight": "0"/, 'Returned reports in flight.');
like($response, qr/"sendReportOperations": "0"/, 'Returned sent report operations.');

################################################################################

sub http_noop_server {
    my ($t, $port, $file) = @_;
    my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
        or die "Can't create test server socket: $!\n";
    local $SIG{PIPE} = 'IGNORE';
    $server->run();
}

################################################################################
