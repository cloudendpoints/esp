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
use src::nginx::t::Auth;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework
use JSON::PP;
use Data::Dumper;

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $CloudTracePort = ApiManager::pick_port();
my $MetadataPort = ApiManager::pick_port();
my $PubkeyPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(41);

my $config = ApiManager::get_bookstore_service_config . <<"EOF";
authentication {
  providers {
    id: "test_auth"
    issuer: "628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l\@developer.gserviceaccount.com"
    jwks_uri: "http://127.0.0.1:${PubkeyPort}/pubkey"
  }
  rules {
    selector: "ListShelves"
    requirements {
      provider_id: "test_auth"
      audiences: "ok_audience_1,ok_audience_2"
    }
  }
}
quota {
 metric_rules [
   {
     selector: "ListShelves"
     metric_costs: [
       {
         key: "metrics_first"
         value: 2
       },
       {
         key: "metrics_second"
         value: 1
       }
     ]
   }
 ]
}
producer_project_id: "api-manager-project"
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

$t->write_file('service.pb.txt', $config);

$t->write_file('server_config.pb.txt', <<"EOF");
cloud_tracing_config {
  url_override: "http://127.0.0.1:${CloudTracePort}"
}
EOF

$t->write_file_expand('nginx.conf', <<"EOF");
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
        server_config server_config.pb.txt;
        on;
      }
      proxy_pass http://127.0.0.1:${BackendPort};
    }
  }
}
EOF

my $report_done = 'report_done';
my $trace_done = 'trace_done';

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);
$t->run_daemon(\&cloudtrace, $t, $CloudTracePort, 'cloudtrace.log', $trace_done);
$t->run_daemon(\&metadata, $t, $MetadataPort, 'metadata.log');
my $pkey_x509 = Auth::get_public_key_x509;
$t->run_daemon(\&pubkey, $t, $PubkeyPort, $pkey_x509, 'pubkey.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${CloudTracePort}"), 1, 'Cloud trace socket ready.');
is($t->waitforsocket("127.0.0.1:${MetadataPort}"), 1, 'Metadata socket ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');

$t->run();

################################################################################

# This request triggers trace.
my $trace_id = 'e133eacd437d8a12068fd902af3962d8';
my $parent_span_id = '12345678';
my $token = Auth::get_auth_token('./src/nginx/t/matching-client-secret.json', 'ok_audience_1');
my $response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
Authorization: Bearer $token
X-Cloud-Trace-Context: ${trace_id}/${parent_span_id};o=1

EOF

my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;
like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body, <<'EOF', 'Shelves returned in the response body.');
Shelves data.
EOF

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
is($t->waitforfile("$t->{_testdir}/${trace_done}"), 1, 'Trace body file ready.');
$t->stop_daemons();

my @trace_requests = ApiManager::read_http_stream($t, 'cloudtrace.log');


# Verify there is only one trace request.
is(scalar @trace_requests, 1, 'Cloud Trace received 1 request.');

my $trace_request = shift @trace_requests;
is($trace_request->{verb}, 'PATCH', 'Cloud Trace: request is PATCH.');
is($trace_request->{uri}, '/v1/projects/api-manager-project/traces',
    'Trace request was called with correct project id in url.');

my $json_obj = decode_json($trace_request->{body});
print Dumper $json_obj;

is($json_obj->{traces}->[0]->{projectId}, 'api-manager-project', 'Project ID in body is correct.');
is($json_obj->{traces}->[0]->{traceId}, $trace_id, 'Trace ID matches the provided one.');

# span 0: root
is($json_obj->{traces}->[0]->{spans}->[0]->{name},
    'endpoints-test.cloudendpointsapis.com/ListShelves',
    'Root trace span name is set to method name of ListShelves');
is($json_obj->{traces}->[0]->{spans}->[0]->{kind}, 'RPC_SERVER', 'Trace span kind is RPC_SERVER');
is($json_obj->{traces}->[0]->{spans}->[0]->{parentSpanId}, $parent_span_id,
    'Parent span of root should be the provided one');
my $agent = $json_obj->{traces}->[0]->{spans}->[0]->{labels}->{'trace.cloud.google.com/agent'};
is($agent, 'esp/' . ServiceControl::get_version(), 'Agent is set to "esp/xxx".');
my $rootid = $json_obj->{traces}->[0]->{spans}->[0]->{spanId};

# span 1: FetchAccessToken
is($json_obj->{traces}->[0]->{spans}->[1]->{name}, 'FetchAccessToken',
    'Next trace span is FetchAccessToken');
is($json_obj->{traces}->[0]->{spans}->[1]->{parentSpanId}, $rootid,
    'Parent of FetchAccessToken span is root');

# span 2: CheckAuth
is($json_obj->{traces}->[0]->{spans}->[2]->{name}, 'CheckAuth',
    'Next trace span is CheckAuth');
is($json_obj->{traces}->[0]->{spans}->[2]->{parentSpanId}, $rootid,
    'Parent of CheckAuth span is root');
my $check_auth_id = $json_obj->{traces}->[0]->{spans}->[2]->{spanId};

# span 2: HttpFetch
is($json_obj->{traces}->[0]->{spans}->[3]->{name}, 'HttpFetch',
    'Next trace span is HttpFetch');
is($json_obj->{traces}->[0]->{spans}->[3]->{parentSpanId}, $check_auth_id,
    'Parent of HttpFetch span is CheckAuth');

# span 4: CheckServiceControl
is($json_obj->{traces}->[0]->{spans}->[4]->{name}, 'CheckServiceControl',
    'Next trace span is CheckServiceControl');
is($json_obj->{traces}->[0]->{spans}->[4]->{parentSpanId}, $rootid,
    'Parent of CheckServiceControl span is root');
my $check_service_control_id = $json_obj->{traces}->[0]->{spans}->[4]->{spanId};

# span 5: CheckServiceControlCache
is($json_obj->{traces}->[0]->{spans}->[5]->{name}, 'CheckServiceControlCache',
    'Next trace span is CheckServiceControlCache');
is($json_obj->{traces}->[0]->{spans}->[5]->{parentSpanId}, $check_service_control_id,
    'Parent of CheckServiceControlCache span is CheckServiceControl');
my $check_service_control_cache_id = $json_obj->{traces}->[0]->{spans}->[5]->{spanId};

# span 6: Call ServiceControl server
is($json_obj->{traces}->[0]->{spans}->[6]->{name}, 'Call ServiceControl server',
    'Next trace span is Call ServiceControl server');
is($json_obj->{traces}->[0]->{spans}->[6]->{parentSpanId}, $check_service_control_cache_id,
    'Parent of Call ServiceControl sever span is CheckServiceControlCache');

# span 7: QuotaControl
is($json_obj->{traces}->[0]->{spans}->[7]->{name}, 'QuotaControl',
    'Next trace span is QuotaControl');
is($json_obj->{traces}->[0]->{spans}->[7]->{parentSpanId}, $rootid,
    'Parent of QuotaControl span is root');
my $quota_control_id = $json_obj->{traces}->[0]->{spans}->[7]->{spanId};

# span 8: QuotaServiceControlCache
is($json_obj->{traces}->[0]->{spans}->[8]->{name}, 'QuotaServiceControlCache',
    'Next trace span is QuotaServiceControlCache');
is($json_obj->{traces}->[0]->{spans}->[8]->{parentSpanId}, $quota_control_id,
    'Parent of QuotaServiceControlCache span is QuotaControl');

# span 9: Backend
is($json_obj->{traces}->[0]->{spans}->[9]->{name}, 'Backend',
    'Next trace span is Backend');
is($json_obj->{traces}->[0]->{spans}->[9]->{parentSpanId}, $rootid,
    'Parent of Beckend span is root');
my $backend_span_id = $json_obj->{traces}->[0]->{spans}->[9]->{spanId};

my @bookstore_requests = ApiManager::read_http_stream($t, 'bookstore.log');
is(scalar @bookstore_requests, 1, 'Bookstore received 1 request.');

my $bookstore_request = shift @bookstore_requests;
is($bookstore_request->{verb}, 'GET', 'backend received a get');
is($bookstore_request->{path}, '/shelves', 'backend received get /shelves');
my $backend_trace_header = $bookstore_request->{headers}->{'x-cloud-trace-context'};
isnt($backend_trace_header, undef, 'X-Cloud-Trace-Context was received');
is($backend_trace_header, $trace_id . '/' . $backend_span_id . ';o=1',
    'X-Cloud-Trace-Context header is correct.');


################################################################################

sub servicecontrol {
  my ($t, $port, $file, $done) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:allocateQuota', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:report', sub {
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

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('GET', '/shelves', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

Shelves data.
EOF
  });

  $server->run();
}

################################################################################

sub cloudtrace {
  my ($t, $port, $file, $done) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
      or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('PATCH', '/v1/projects/api-manager-project/traces', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

{}
EOF
    $t->write_file($done, ':trace done');
  });

  $server->run();
}

################################################################################

sub metadata {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

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

sub pubkey {
  my ($t, $port, $pkey, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/pubkey', <<"EOF");
HTTP/1.1 200 OK
Connection: close

$pkey
EOF

  $server->run();
}

################################################################################
