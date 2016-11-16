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
use src::nginx::t::ServiceControl;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework
use JSON::PP;

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $MetadataPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(36);

# Save service name in the service configuration protocol buffer file.
my $config = ApiManager::get_echo_service_config .
             ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF";
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF
$t->write_file('service.pb.txt', $config);

ApiManager::write_file_expand($t, 'nginx.conf', <<"EOF");
%%TEST_GLOBALS%%
daemon off;
events {
  worker_connections 32;
}
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

sub run_test_case {
  my ($t, $test_case) = @_;

  my $name = $test_case->{name};

  my $report_done = "report-done-${name}";
  my $bookstore_log = "bookstore-${name}.log";
  my $servicecontrol_log = "servicecontrol-${name}.log";
  my $metadata_log = "metadata-${name}.log";

  $t->run_daemon(\&bookstore, $t, $BackendPort, $bookstore_log, $test_case->{platform});
  $t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, $servicecontrol_log, $report_done);
  $t->run_daemon(\&metadata, $t, $MetadataPort, $test_case->{metadata_key},
                 $test_case->{metadata_value}, $metadata_log);

  is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, "${name}: Bookstore socket ready.");
  is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, "${name}: Service control socket ready.");
  is($t->waitforsocket("127.0.0.1:${MetadataPort}"), 1, "${name}: Metadata server socket ready.");

  $t->run();

  my $response = ApiManager::http_get($NginxPort,'/echo?key=this-is-an-api-key');

  is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, "${name}: Report body file ready.");
  $t->stop();
  $t->stop_daemons();

  my @servicecontrol_requests = ApiManager::read_http_stream($t, $servicecontrol_log);
  is(scalar @servicecontrol_requests, 2, "${name}: Service control was called twice.");

  # :check
  my $r = shift @servicecontrol_requests;
  like($r->{uri}, qr/:check$/, "${name}: check was called");

  # :report
  $r = shift @servicecontrol_requests;
  like($r->{uri}, qr/:report$/, "${name}: report was called");

  my $report_json = decode_json(ServiceControl::convert_proto($r->{body}, 'report_request', 'json'));
  my $operations = $report_json->{operations};
  is(@{$operations}, 1, "${name}: There is one operation");
  is($operations->[0]->{labels}->{'servicecontrol.googleapis.com/service_agent'},
     ServiceControl::service_agent(), "${name}: service_agent is correct.");
  is($operations->[0]->{labels}->{'servicecontrol.googleapis.com/platform'},
     $test_case->{platform}, "${name}: Platform is correct ($test_case->{platform}).");

  my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;
  like($response_headers, qr/HTTP\/1\.1 200 OK/, "${name}: Request returned 200");
  is($response_body, <<"EOF", "${name}: method response body");
{ "platform": "$test_case->{platform}" }
EOF
}

my @test_cases = (
  {
    name => 'gae',
    metadata_key => 'gae_server_software',
    metadata_value => 'Google App Engine/1.9.38',
    platform => 'GAE Flex',
  },
  {
    name => 'gke',
    metadata_key => 'kube-env',
    metadata_value => 'Kubernetes environment',
    platform => 'GKE',
  },
  {
    name => 'gce',
    metadata_key => 'insignificant-key',
    metadata_value => 'value',
    platform => 'GCE',
  }
);

for my $test_case ( @test_cases ) {
  run_test_case($t, $test_case);
}

################################################################################

sub servicecontrol {
  my ($t, $port, $file, $done) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";

  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('POST', '/v1/services/echo.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/echo.cloudendpointsapis.com:report', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
    $t->write_file($done, ':report done');
  });

  $server->run();
}

################################################################################

sub metadata {
  my ($t, $port, $metadata_key, $metadata_value, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/computeMetadata/v1/?recursive=true', <<EOF);
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

{
  "instance": {
    "attributes": {
      "${metadata_key}": "${metadata_value}"
    },
    "hostname": "gae-default-20150921t180445-inqp.c.esp-test-app.internal",
    "zone": "projects/345623948572/zones/us-west1-a"
  },
  "project": {
    "numericProjectId": 345623948572,
    "projectId": "esp-test-app"
  }
}
EOF

  $server->on('GET', '/computeMetadata/v1/instance/service-accounts/default/token', <<'EOF');
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

{
 "access_token":"ya29.7gFRTEGmovWacYDnQIpC9X9Qp8cH0sgQyWVrZaB1Eg1WoAhQMSG4L2rtaHk1",
 "expires_in":200,
 "token_type":"Bearer"
}
EOF

  $server->run();
}

################################################################################

sub bookstore {
  my ($t, $port, $file, $platform) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/echo', <<"EOF");
HTTP/1.1 200 OK
Connection: close

{ "platform": "${platform}" }
EOF

  $server->run();
}

################################################################################

