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

use src::nginx::t::ApiManager;   # Must be first (sets up import path to the Nginx test module)
use src::nginx::t::HttpServer;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(14);

# Save service name in the service configuration protocol buffer file.
my $config = ApiManager::get_bookstore_service_config . <<"EOF";
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF
$t->write_file('service.pb.txt', $config);

# The private key has been revoked immediately after creation.
my $PrivateKey = <<EOF;
-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDeV30WtIk/KC4B
nB5xRB/IWbiezUzKZZ9RPpquAwKdl3MC0Ncr6stWB1TW87+38nCwbO54tS5365FH
PHLowOMIfIFaj7YXWa6F8UdTn5CT+lgV4zvyBG8upuxSG2j/ZngfTx1J2GaPsRXu
trfmyS2LFOp/SVNvQOv+ue2a+4xwnSiPnwry+IoMPQ6zmOGDYE6FQAkwgkul2nff
UZN+KXXfSAaQ4ykYfmAX3um5YfoXdYdjM4lZolZGT4dcRYmlUzLuX/mrvgTWN1rY
pS37vH5F6HLY06bL+SS4IZO40OHGD5DcEDgNznFzLs9PS+PKYoJMwvytQLssuE7m
hUFWTPd/AgMBAAECggEAeiXOTr739353Z+MAxnEKlOLVjkhzO24AzH18NMTqlvEw
+gcJbtMayjRU45IdbUz/o6z0hdPjp3X+5gfLiRsOnwbneX0iKEnYmzUmXkZ3AxSx
d7TSpe7Rtet2QNVWJZmBvJGWIY+FKMO8rNwsw9kZ8CKZgTwjXidofxRd+JrhWKu4
DOZBbKcsDXDZqhSPdpnhbSSxidlpqorwZucTJzX7AHDngnNTqX/miElK7uhT7F7C
L/nZvfzWKN+SM3DIpVEcQDkOxidZXvF/MOltA9Sku86c0skPsBP6xkMm7xOU9M+J
UAJkGt/fgrqDnr8ln1w85Vb4BQvIdNlJkcbLl6YrAQKBgQDvYI5qAvswskQPtOcM
8UzeLZYhzh54lW8HUSRrupjclYIDGpuuamSNEABr9YG/lJW1P/DjcXm3yGw/IZSn
dX04bmf7I8YVeSwfIwtRUhkipns8GJPmkLyPQXMEMsOafaEVWb/D7RQCsOwD7w0Y
afjm2eeSaTB4jH1SQMTHTQNHHwKBgQDtyBdTBB8wa4r91h8ehh78XaZUYbb9f7TE
0FWkHxSxXFioOhsRPguafPaO4qeU8xWkWzQ+m/l9qImVvSFBIyrANtY92NclRZZ+
unXI4xPlY56GzaGoGua6TryS3SZOec1Y9MOcj/ZXayGAXgkfhDw91lE1kGa9US4A
H7vjTvwjoQKBgQCKd9oprKvNEXGZfFWjAPosE0ajK24o5pghLWjaAEhoYPuh/ARO
MjEUAEueJ5f0UGkBPYmEp6F3FDK5mh05eRcES6lOdvUgesVxBX6IfesYRiFHNBhp
6ROJ8pwrs4m+lilWBmKNXViT7e+4ntF+a96U+zufT8XAFdRwfhLWDtB0lQKBgC7M
2iJhxk2+bP3m/fsBBOpA+HLVRLICR68RHjoOUAFUnrKFtTwgjSIcBF89JyS+73yI
4vCLvIBKAsJxFjF+3XQ1ltdXbYkNeEB7LKGcaBcXE2WO5YlzugPjaWQymM6LVKp0
imevQhoUoORmHb+RRFYbb6JuSHpslvQ5Sr7DlgbhAoGBAK/mtGgVjfBZkaAgTOll
odc++YmBY/s1enFixBVGDlWyv2M9CFbUjxubDQs11K2EuGrQbBzUaImgIx5PZuLB
vkgfXhuwm7eDX06a9QvbXLDSvZ77EK3y8orFChf8MN1ONsuIXqgyqrmQ3tiKVyan
cUmegK2Gx9nzH1gK4c8EFHat
-----END PRIVATE KEY-----
EOF

# Escape newlines.
$PrivateKey =~ s/\n/\\n/g;

$t->write_file('client-secret.json', <<"EOF");
{ "private_key_id": "1d4f0903ff3a4af3d07f0d7b2bee8e0ecd1198a3",
  "private_key": "${PrivateKey}",
  "client_email": "developer.gserviceaccount.com",
  "client_id": "apps.googleusercontent.com",
  "type": "service_account"
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
        servicecontrol_secret client-secret.json;
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

my $response = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key');

$t->stop_daemons();

my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body, <<'EOF', 'Shelves returned in the response body.');
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

my @requests = ApiManager::read_http_stream($t, 'bookstore.log');
is(scalar @requests, 1, 'Bookstore received one request');

my $r = shift @requests;
is($r->{verb}, 'GET', 'Bookstore request was get');
is($r->{uri}, '/shelves?key=this-is-an-api-key', 'Bookstore uri was /shelves');
is($r->{headers}->{host}, "127.0.0.1:${BackendPort}", 'Bookstore request had Host header');


@requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @requests, 1, 'Service control received one request');

$r = shift @requests;
is($r->{verb}, 'POST', 'Service control request was get');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', ':check was called.');
is($r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}", ':check request had Host header');
like($r->{headers}->{authorization}, qr/Bearer \S+$/, ':check request was authenticated');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', ':check request was protocol buffer');

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/shelves?key=this-is-an-api-key', <<'EOF');
HTTP/1.1 200 OK
Connection: close

{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

  $server->run();
}

sub servicecontrol {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', <<'EOF');
HTTP/1.1 200 OK
Connection: close

EOF

  $server->run();
}

################################################################################
