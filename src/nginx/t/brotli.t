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

select STDERR; $| = 1;
select STDOUT; $| = 1;

my $t = Test::Nginx->new()->has(qw/http proxy brotli/)->plan(6);

$t->write_file_expand('nginx.conf', <<'EOF');

%%TEST_GLOBALS%%

daemon off;

events {
}

http {
    %%TEST_GLOBALS_HTTP%%

    server {
        listen       127.0.0.1:8080;
        server_name  localhost;
        location / {
            brotli on;
        }
        location /proxy/ {
            brotli on;
            proxy_pass http://127.0.0.1:8080/local/;
        }
        location /local/ {
            brotli off;
            alias %%TESTDIR%%/;
        }
    }
}

EOF

$t->write_file('index.html', 'X' x 64);

$t->run();

###############################################################################

my $r;

$r = http_brotli_request('/');
like($r, qr/^Content-Encoding: br/m, 'br');

$r = http_brotli_request('/proxy/');
like($r, qr/^Content-Encoding: br/m, 'br proxied');

# Accept-Ranges headers should be cleared

unlike(http_brotli_request('/'), qr/Accept-Ranges/im, 'cleared accept-ranges');
unlike(http_brotli_request('/proxy/'), qr/Accept-Ranges/im,
	'cleared headers from proxy');

# HEAD requests should return correct headers

like(http_brotli_request('/'), qr/Content-Encoding: br/, 'br head');
unlike(http_head('/'), qr/Content-Encoding: br/, 'no br head');

###############################################################################

sub http_brotli_request {
	my ($url) = @_;
	my $r = http(<<EOF);
GET $url HTTP/1.1
Host: localhost
Connection: close
Accept-Encoding: br

EOF
}


###############################################################################
