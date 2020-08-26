#!/usr/bin/perl

# Copyright (C) 2002-2016 Igor Sysoev
# Copyright (C) 2011-2016 Nginx, Inc.
# Copyright (C) 2020 Google Inc.
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

# tests for the nginx latency status module
# run with TEST_NGINX_BINARY={path to nginx binary} prove http_latency_status.t

###############################################################################

use warnings;
use strict;

use Test::More;

BEGIN { use FindBin; chdir($FindBin::Bin); }

use lib "nginx-tests/lib";
use Test::Nginx;
use JSON::PP qw(decode_json);
###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

my $t = Test::Nginx->new()->has(qw/http proxy rewrite/)->plan(17);

$t->write_file_expand('nginx.conf', <<'EOF');
%%TEST_GLOBALS%%
daemon off;
events {
}
http {
    %%TEST_GLOBALS_HTTP%%
    store_latency 2 100 1;
    upstream u {
        server 127.0.0.1:8082;
    }
    server {
        listen       127.0.0.1:8080;
        server_name  localhost;
        location /status {
	    latency_stub_status;
        }
        location /success {
            return 200 "TEST\n";
	    record_latency on;
        }
        location /pass {
            proxy_pass http://u/;
	    record_latency on;
        }
        location /error {
            return 404;
	    record_latency on;
        }
	location /pass-to-monitored {
	    proxy_pass http://localhost:8081;
	}
    }
    server {
        listen       127.0.0.1:8082;
        server_name  localhost;
        location / {
            return 200 "TEST\n";
        }
    }
    server {
        listen       127.0.0.1:8081;
	server_name  localhost;
	record_latency on;
	location / {
	    return 200 "TEST\n";
	}
    }
}
EOF

$t->run();

my $initial_status = "{\n  \"accepted_connections\": 1,\n".
                       "  \"handled_connections\": 1,\n".
		       "  \"active_connections\": 1,\n".
		       "  \"requests\": 1,\n".
		       "  \"reading_connections\": 0,\n".
		       "  \"writing_connections\": 1,\n".
		       "  \"waiting_connections\": 0,\n".
		       "  \"request_latency\":{\n".
		       "    \"latency_sum\": 0,\n".
		       "    \"request_count\": 0,\n".
		       "    \"sum_squares\": 0,\n".
		       "    \"distribution\": [0, 0, 0]\n".
		       "  },\n".
		       "  \"upstream_latency\":{\n".
		       "    \"latency_sum\": 0,\n".
                       "    \"request_count\": 0,\n".
                       "    \"sum_squares\": 0,\n".
                       "    \"distribution\": [0, 0, 0]\n".
		       "  },\n".
		       "  \"websocket_latency\":{\n".
		       "    \"latency_sum\": 0,\n".
                       "    \"request_count\": 0,\n".
                       "    \"sum_squares\": 0,\n".
                       "    \"distribution\": [0, 0, 0]\n".
		       "  },\n".
		       "  \"latency_bucket_bounds\": [ 100, 200]\n".
		       "}\n";

like(http_get('/status'), qr/\Q${initial_status}\E/, 'check initial stats twice');

http_get('/success');
my $status = decode_json(http_get_body('/status'));

is($status->{'request_latency'}{'request_count'}, 1, 'request count after 1 monitored request');
is(get_distribution_sum($status, 'request_latency'), 1, 'The sum of the distribution bucket counts after 1 monitored request');
is($status->{'upstream_latency'}{'request_count'}, 0, 'upstream request count when there have been no proxied requests');
is(get_distribution_sum($status, 'upstream_latency'), 0, 'sum of upstream distribution bucket counts when there haven been no proxied requests');
is($status->{'websocket_latency'}{'request_count'}, 0, 'websocket request count when there have been no websocket requests');
is(get_distribution_sum($status, 'websocket_latency'), 0, 'sum of websocket distribution bucket counts when there have been no websocket requests');

http_get('/error');
$status = decode_json(http_get_body('/status'));

is($status->{'request_latency'}{'request_count'}, 2, 'erroring requests add to the request count');
is(get_distribution_sum($status, 'request_latency'), 2, 'erroring requests add to distribution bucket counts');

http_get('/pass');
$status = decode_json(http_get_body('/status'));

is($status->{'request_latency'}{'request_count'}, 3, 'proxied requests add to the request count');
is(get_distribution_sum($status, 'request_latency'), 3, 'proxied requests add to the distribution bucket counts');
is($status->{'upstream_latency'}{'request_count'}, 1, 'proxied requests add to the upstream request count');
is(get_distribution_sum($status, 'upstream_latency'), 1, 'proxied requests add to the upstream distribution bucket counts');
is($status->{'websocket_latency'}{'request_count'}, 0, 'a non-websocket proxied request does not add to the websocket request count');
is(get_distribution_sum($status, 'websocket_latency'), 0, 'non-websocket proxied request does not add to websocket distribution bucket counts');

http_get('/pass-to-monitored');
$status = decode_json(http_get_body('/status'));

is($status->{'request_latency'}{'request_count'}, 4, 'record_latency set at the server level adds to the request count');
is(get_distribution_sum($status, 'request_latency'), 4, 'record_latency set at the server level adds to the distribution bucket counts');

sub get_distribution_sum {
	my ($status, $distribution_name) = @_;

	my $latency_count = 0;
	my @distribution = $status->{$distribution_name}{'distribution'};
	for(my $i = 0; $i < @distribution; $i++) {
		$latency_count += $status->{$distribution_name}{'distribution'}[$i];
	}
	return $latency_count;
}

sub http_get_body {
	my ($uri) = @_;

	return undef if !defined $uri;

	my $text = http_get($uri);

	if ($text !~ /(.*?)\x0d\x0a?\x0d\x0a?(.*)/ms) {
		return undef;
	}

	return $2;
}
