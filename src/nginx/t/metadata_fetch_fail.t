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

use src::nginx::t::ApiManager; # Must be first (sets up import path to the Nginx test module)
use src::nginx::t::HttpServer;
use src::nginx::t::ServiceControl;
use Test::Nginx; # Imports Nginx's test module
use Test::More;  # And the test framework

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $MetadataPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(8);

my $config = ApiManager::get_bookstore_service_config . <<"EOF";
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

$t->write_file('service.pb.txt', $config);
ApiManager::write_file_expand($t, 'nginx.conf', <<"EOF");
%%TEST_GLOBALS%%
daemon off;
events { worker_connections 32; }
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
        %%TEST_CONFIG%%
        on;
      }
      proxy_pass http://127.0.0.1:${BackendPort};
    }
  }
}
EOF


sub test_metadata {
    my ($sleepLength, $wantReqHeader, $wantReqBody) = @_;
    my $backend_log = 'backend.log';

    $t->run_daemon(\&backends, $t, $BackendPort, $backend_log);
    $t->run_daemon(\&metadata, $t, $MetadataPort, 'metadata.log');
    is($t->waitforsocket("127.0.0.1:${MetadataPort}"), 1, 'Metadata socket ready.');

    $t->run();

    ################################################################################

    my $shelves1 = ApiManager::http_get($NginxPort, '/shelves');

    my ($shelves_headers1, $shelves_body1) = split /\r\n\r\n/, $shelves1, 2;
    like($shelves_headers1, qr/HTTP\/1\.1 500 Internal Server Error/, '/shelves returned HTTP 500.');
    like($shelves_body1, qr/Failed to fetch service account token/, 'Returned Failure Status');

    ################################################################################
    # if no sleep, the service account token still doesn't get out of last failed
    # fetch so it will fail.
    sleep $sleepLength;

    my $shelves2 = ApiManager::http_get($NginxPort, '/shelves');

    $t->stop();
    $t->stop_daemons();

    my ($shelves_headers2, $shelves_body2) = split /\r\n\r\n/, $shelves2, 2;
    like($shelves_headers2, $wantReqHeader, 'Got expected request header');
    like($shelves_body2, $wantReqBody, 'Got expected request body');
}
# Fail the first request by failed fetch and do the second request after 2s, which
# also get failed since the failed fetch status doesn't expire.
test_metadata(2, qr/HTTP\/1\.1 500 Internal Server Error/, qr/Failed to fetch service account token/);
# Fail the first request by failed fetch and do the second request after sleeping
# 6s , which will get the token.
test_metadata(6, qr/HTTP\/1.1 401 Unauthorized/, qr/Method doesn't allow unregistered callers/);

################################################################################

sub metadata {
    my ($t, $port, $file) = @_;
    my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
        or die "Can't create test server socket: $!\n";
    my $request_count = 0;

    local $SIG{PIPE} = 'IGNORE';
    $server->on_sub('GET', '/computeMetadata/v1/instance/service-accounts/default/token', sub {
        my ($headers, $body, $client) = @_;
        $request_count++;

        # The retry time is 5 and it would be 6 times for the first failed fetch.
        if ($request_count < 7) {
            return;
        }

        # Only the 7th request will get the token.
        print $client <<'EOF';
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

{
 "access_token":"ya29.7gFRTEGmovWacYDnQIpC9X9Qp8cH0sgQyWVrZaB1Eg1WoAhQMSG4L2rtaHk1",
 "expires_in":200,
 "token_type":"Bearer"
}
EOF
    });

    $server->run();
}

################################################################################