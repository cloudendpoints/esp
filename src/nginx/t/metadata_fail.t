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

################################################################################

# Port assignments
my $NginxPort = 8080;
my $BackendPort = 8081;
my $ServiceControlPort = 8082;
my $MetadataPort = 8083;

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(11);

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
    location = /~~endpoints~subrequest~~ {
        internal;
        proxy_pass \$endpoints_subrequest_url;
    }
  }
}
EOF

sub no_check_call {
  my (@requests) = @_;
  foreach my $r (@requests) {
    if ($r->{path} =~ qr/:check$/) {
      return 0;
    }
  }
  return 1;
}

# Metadata server is running but returns an HTTP 404 status
sub test_metadata_404 {
  my $report_done = 'report_done_404';
  my $backend_log = 'backend_404.log';
  my $servicecontrol_log = 'servicecontrol_404.log';

  $t->run_daemon(\&backends, $t, $BackendPort, $backend_log);
  $t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, $servicecontrol_log, $report_done);
  $t->run_daemon(\&metadata, $t, $MetadataPort, 'metadata.log');

  is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'bookstore socket ready.');
  is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
  is($t->waitforsocket("127.0.0.1:${MetadataPort}"), 1, 'Metadata socket ready.');

  $t->run();

  ################################################################################

  my $shelves1 = http_get('/shelves');
  is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report succeeded');

  $t->stop();
  $t->stop_daemons();

  my ($shelves_headers1, $shelves_body1) = split /\r\n\r\n/, $shelves1, 2;
  like($shelves_headers1, qr/HTTP\/1\.1 503 Service Temporarily Unavailable/, '/shelves returned HTTP 503.');

  is($t->read_file($backend_log), '', 'Backend was not called.');

  my @servicecontrol_requests = ApiManager::read_http_stream($t, $servicecontrol_log);
  ok(no_check_call(@servicecontrol_requests), 'Service control check was not called after metadata error.');
}

test_metadata_404();

################################################################################

# Metadata server is not running.
sub test_metadata_not_running {
  my $report_done = 'report_done_no_metadata';
  my $backend_log = 'backend_no_metadata.log';
  my $servicecontrol_log = 'servicecontrol_no_metadata.log';

  $t->run_daemon(\&backends, $t, $BackendPort, $backend_log);
  $t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, $servicecontrol_log, $report_done);
  is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
  is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service controlsocket ready.');
  $t->run();

  my $shelves2 = http_get('/shelves');
  is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report succeeded.');

  $t->stop();
  $t->stop_daemons();

  my ($shelves_headers2, $shelves_body2) = split /\r\n\r\n/, $shelves2, 2;
  like($shelves_headers2, qr/HTTP\/1\.1 503 Service Temporarily Unavailable/, '/shelves returned HTTP 503 (metadata down)');

  is($t->read_file($backend_log), '', 'Backend was not called.');

  ok(no_check_call(ApiManager::read_http_stream($t, $servicecontrol_log)),
                   'Service control check was not called after metadata connection failure.');
}

test_metadata_not_running();

################################################################################

sub checkfile{
    if (-e $_[0]) { return 1;}
    else { return 0; }
}

sub backends {
  my ($t, $port, $file) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';
  $server->run();
}

sub metadata {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/computeMetadata/v1/?recursive=true', <<'EOF');
HTTP/1.1 404 Not Found
Metadata-Flavor: Google
Content-Type: application/json

{
 "error": "Not Found"
}
EOF

  $server->run();
}

################################################################################

sub servicecontrol {
  my ($t, $port, $file, $done) = @_;

  # Save requests (last argument).
  my $server = HttpServer->new($ServiceControlPort, $t->testdir() . '/' . $file)
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
    $t->write_file($done, ':report done');
  });

  $server->run();
}

################################################################################
