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
use src::nginx::t::Auth;
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

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(55);

$t->write_file('service.pb.txt',
               ApiManager::get_bookstore_service_config .
               ApiManager::read_test_file('testdata/logs_metrics.pb.txt') .
               <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

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

my $report_done = 'report_done';

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);
$t->run_daemon(\&metadata, $t, $MetadataPort, 'metadata.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${MetadataPort}"), 1, 'Metadata socket ready.');

$t->run();

################################################################################

# Call two methods. One returns 200, the other 404.
my $rshelf = ApiManager::http_get($NginxPort,'/shelves/1?key=an-api-key');
my $rbook  = ApiManager::http_get($NginxPort,'/shelves/1/books/2?key=an-api-key');

# Wait for :report body.
is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ');

$t->stop_daemons();

# Verify the responses.
like($rshelf, qr/HTTP\/1\.1 200 OK/, '/shelves/1 returned 200 OK');
like($rshelf, qr/"name": "shelves\/1"/, '/shelves/1 returned resource');
like($rbook, qr/HTTP\/1\.1 404 Not Found/,
     '/shelves/1/books/2 returned 404 Not Found');
like($rbook, qr/"name": "shelves\/1\/books\/2".*"message": "Not found."/,
     '/shelves/1/books/2 returned error.');

# Verify service control :check and :report were called.
my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 3, 'Service control was called three times');

# :check 1
my $r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':check 1 request was POST');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check',
   ':check 1 was called');

# :check 2
my $r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':check 2 request was POST');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check',
   ':check 2 was called');

# :report
$r = shift @servicecontrol_requests;
is($r->{verb}, 'POST', ':report request was POST');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:report',
   ':report was called');

# Verify the :report body contents.
my $report = decode_json(ServiceControl::convert_proto($r->{body}, 'report_request', 'json'));

sub find_in_array {
  my ($name, $value, $array_ref) = @_;
  foreach my $op (@{$array_ref}) {
    if (exists $op->{$name} && $op->{$name} eq $value) {
      return $op;
    }
  }
  return undef;
}

#
# Check GetShelf operation and its metrics and labels.
#
my $get_shelf = find_in_array('operationName', 'GetShelf', $report->{operations});
isnt($get_shelf, undef, 'Found GetShelf operation');

# Declare variales used below.
my ($mn, $metrics, $metric, $labels);

$labels = $get_shelf->{labels};
isnt($labels, undef, 'GetShelf operation has labels');
is($labels->{'/credential_id'}, 'apiKey:an-api-key',
   'GetShelf credential id is an API key.');
is($labels->{'/protocol'}, 'http', 'GetShelf protocol is http');
is($labels->{'/response_code'}, '200', 'GetShelf response code is 200');
is($labels->{'/response_code_class'}, '2xx', 'GetShelf response code class is 2xx');

$metrics = $get_shelf->{metricValueSets};
isnt($metrics, undef, 'GetShelf has metrics');

# Check by_consumer Metrics.
$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/request_count';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "GetShelf has $mn metric.");
is($metric->{metricValues}[0]->{int64Value}, '1', "GetShelf $mn is 1.");

$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/request_sizes';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "GetShelf has $mn metric.");
is($metric->{metricValues}[0]->{distributionValue}->{count}, '1',
   "GetShelf $mn has 1 value.");

$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/total_latencies';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "GetShelf has $mn metric.");
is($metric->{metricValues}[0]->{distributionValue}->{count}, '1',
   "GetShelf $mn has 1 value.");

$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/backend_latencies';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "GetShelf has $mn metric.");
is($metric->{metricValues}[0]->{distributionValue}->{count}, '1',
   "GetShelf $mn has 1 value.");

$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/request_overhead_latencies',
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "GetShelf has $mn metric.");
is($metric->{metricValues}[0]->{distributionValue}->{count}, '1',
   "GetShelf $mn has 1 value.");

# Error metrics are not populated.
$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/error_count';
$metric = find_in_array('metricName', $mn, $metrics);
is($metric, undef, "GetShelf does not have $mn metric.");

#
# Check GetBook operation and its metrics and labels.
#
my $get_book = find_in_array('operationName', 'GetBook', $report->{operations});
isnt($get_book, undef, 'Found GetBook operation.');

$labels = $get_book->{labels};
isnt($labels, undef, 'GetBook operation has labels');
is($labels->{'/credential_id'}, 'apiKey:an-api-key',
    'GetBook credential id is an API key.');
is($labels->{'/protocol'}, 'http', 'GetBook protocol is http.');
is($labels->{'/response_code'}, '404', 'GetBook response code is 404.');
is($labels->{'/error_type'}, '4xx', 'GetBook error type is 4xx.');
is($labels->{'/response_code_class'}, '4xx', 'GetBook response code class is 4xx.');

$metrics = $get_book->{metricValueSets};
isnt($metrics, undef, 'GetBook has metrics.');

# Check by_consumer metrics.
$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/request_count';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "GetBook has $mn metric.");
is($metric->{metricValues}[0]->{int64Value}, '1', "GetBook $mn is 1.");

$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/request_sizes';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "GetBook has $mn metric.");
is($metric->{metricValues}[0]->{distributionValue}->{count}, '1',
   "GetBook $mn has 1 value.");

$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/response_sizes';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "GetBook has $mn metric.");
is($metric->{metricValues}[0]->{distributionValue}->{count}, '1',
   "GetBook $mn has 1 value.");

$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/total_latencies';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "GetBook has $mn metric.");
is($metric->{metricValues}[0]->{distributionValue}->{count}, '1',
   "GetBook $mn has 1 value.");

$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/backend_latencies';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "GetBook has $mn metric.");
is($metric->{metricValues}[0]->{distributionValue}->{count}, '1',
   "GetBook $mn has 1 value.");

$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/request_overhead_latencies';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "getbook has $mn metric.");
is($metric->{metricValues}[0]->{distributionValue}->{count}, '1',
   "GetBook $mn has 1 value.");

$mn = 'serviceruntime.googleapis.com/api/producer/by_consumer/error_count';
$metric = find_in_array('metricName', $mn, $metrics);
isnt($metric, undef, "GetBook has $mn metric.");
is($metric->{metricValues}[0]->{int64Value}, '1', "GetBook $mn is 1.");

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

  $server->on('GET', '/shelves/1', <<'EOF');
HTTP/1.1 200 OK
Content-Type: application/json

{ "name": "shelves/1", "theme": "Fiction-Fantasy" }
EOF

  $server->on('GET', '/shelves/1/books/2', <<'EOF');
HTTP/1.1 404 Not Found
Content-Type: application/json

{ "name": "shelves/1/books/2", "error": 404, "message": "Not found." }
EOF

  $server->run();
}

################################################################################
# Metadata server.

sub metadata {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create metadata server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/computeMetadata/v1/?recursive=true', <<'EOF' . ApiManager::get_metadata_response_body);
HTTP/1.1 200 OK
Metadata-Flavor: Google
Content-Type: application/json

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
