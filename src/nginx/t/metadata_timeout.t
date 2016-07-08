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
use JSON::PP;
use ServiceControl;

################################################################################

# Port assignments
my $NginxPort = 8080;
my $BackendPort = 8081;
my $ServiceControlPort = 8082;
my $MetadataPort = 8083;

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(13);

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

# Daemon synchronization files.
my $report_done = 'report.done';

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);
$t->run_daemon(\&metadata, $t, $MetadataPort, 'metadata.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${MetadataPort}"), 1, 'Metadata socket ready.');

$t->run();

################################################################################

my $shelves = http_get('/shelves');
my $books = http_get('/shelves/Musicals/books');

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');

$t->stop();
$t->stop_daemons();

like($shelves, qr/^HTTP\/1\.1 401 Unauthorized/, '/shelves returned HTTP 401.');
like($books, qr/^HTTP\/1\.1 401/, '/books returned HTTP 401.');

# Check metadata server log.
my @metadata_requests = ApiManager::read_http_stream($t, 'metadata.log');
is(scalar @metadata_requests, 4, 'Metadata server received all requests.');

# Metadata request 1
my $r = shift @metadata_requests;
is($r->{verb}, 'GET', 'Metadata request 1 was GET');
is($r->{uri}, '/computeMetadata/v1/?recursive=true', 'Metadata request was recursive');

# Check service control requests...
my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 1, 'Service control received 1 request (report).');

# Request 1 :report
$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':report was POST');
is($r->{path}, '/v1/services/endpoints-test.cloudendpointsapis.com:report', ':report path');

# TODO: We shouldn't send unauthenticated requests to service control (if
# metadata server is to be used).
#
# is($r->{headers}->{authorization},
#    'Bearer ya29.7gFRTEGmovWacYDnQIpC9X9Qp8cH0sgQyWVrZaB1Eg1WoAhQMSG4L2rtaHk1',
#    ':report was authenticated');

my $report_json = decode_json(ServiceControl::convert_proto($r->{body}, 'report_request', 'json'));
my $operations = $report_json->{operations};
is(2, @{$operations}, 'There are 2 operations total');

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/shelves', <<'EOF');
HTTP/1.1 200 OK
Content-Type: application/text
Connection: close

Musicals
EOF

  $server->on('GET', '/shelves/Musicals/books', <<'EOF');
HTTP/1.1 200 OK
Content-Type: application/text
Connection: close

Hamilton: The Revolution
EOF

  $server->run();
}

sub servicecontrol {
  my ($t, $port, $file, $done) = @_;

  # Save requests (last argument).
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
    $t->write_file($done, ':report called');
  });

  $server->run();
}

sub metadata {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('GET', '/computeMetadata/v1/?recursive=true', sub {
    my ($headers, $body, $client) = @_;

    # Trigger a timeout.
    sleep 2;

    print $client <<'EOF' . ApiManager::get_metadata_response_body();
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

EOF
  });

  $server->on('GET', '/computeMetadata/v1/instance/service-accounts/default/token', <<'EOF');
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

{
 "access_token":"ya29.7gFRTEGmovWacYDnQIpC9X9Qp8cH0sgQyWVrZaB1Eg1WoAhQMSG4L2rtaHk1",
 "expires_in":100,
 "token_type":"Bearer"
}
EOF

  $server->run();
}

################################################################################
