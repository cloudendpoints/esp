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

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(7);

my $NginxPort = ApiManager::pick_port();

$t->write_file_expand('nginx.conf', <<"EOF");
%%TEST_GLOBALS%%
daemon off;
events { worker_connections 32; }
http {
  %%TEST_GLOBALS_HTTP%%
  server_tokens off;
  endpoints { off; }
  server {
    listen 127.0.0.1:$NginxPort;
    server_name localhost;
    location /status {
      endpoints_status;
    }
  }
}
EOF

$t->run();

################################################################################

my $response = ApiManager::http_get($NginxPort,'/status');
$t->stop_daemons();

like($response, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
like($response, qr/Content-Type: application\/json/, 'Returned expected content type.');
like($response, qr/"nginxServerVersion": "nginx\/\d+\.\d+\.\d+"/, 'Returned expected server entry.');
like($response, qr/"serverVersion": "ESP\/\d+\.\d+\.\d+"/, 'Returned expected server entry.');
like($response, qr/"builtBy": "/, 'Returned build info entry.');
like($response, qr/"requests": "1"/, 'Returned expected request entry.');
like($response, qr/"processId":/, 'Returned expected process ID entry.');

################################################################################
