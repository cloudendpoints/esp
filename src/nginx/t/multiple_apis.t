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

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(15);
$t->write_file('service.json', <<"EOF");
{
    "apis": [
        {
            "name": "test.Bookstore",
            "methods": [
                {
                    "name": "ListShelves",
                    "requestTypeUrl": "type.googleapis.com/google.protobuf.Empty",
                    "responseTypeUrl": "type.googleapis.com/google.protobuf.Empty",
                }
            ],
            "version": "v1"
        },
        {
            "name": "Bookstore",
            "methods": [
                {
                    "name": "ListShelves",
                    "requestTypeUrl": "type.googleapis.com/google.protobuf.Empty",
                    "responseTypeUrl": "type.googleapis.com/google.protobuf.Empty",
                }
            ],
            "version": "v2"
        }
    ],
    "control": {
        "environment": "http://127.0.0.1:${ServiceControlPort}"
    },
    "http": {
        "rules": [
            {
                "get": "/shelves",
                "selector": "test.Bookstore.ListShelves"
            }
        ]
    },
    "id": "2016-08-25r1",
    "name": "endpoints-test.cloudendpointsapis.com",
    "logging": {
        "producerDestinations": [
            {
                "logs": [
                    "endpoints_log"
                ],
                "monitoredResource": "api"
            }
        ]
    },
    "logs": [
        {
            "name": "endpoints_log"
        }
    ],
    "monitoredResources": [
        {
            "labels": [
                {
                    "key": "cloud.googleapis.com/location"
                },
                {
                    "key": "cloud.googleapis.com/uid"
                },
                {
                    "key": "serviceruntime.googleapis.com/api_version"
                },
                {
                    "key": "serviceruntime.googleapis.com/api_method"
                },
                {
                    "key": "serviceruntime.googleapis.com/consumer_project"
                },
                {
                    "key": "cloud.googleapis.com/project"
                },
                {
                    "key": "cloud.googleapis.com/service"
                }
            ],
            "type": "api"
        }
    ]
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
        api service.json;
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

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');

$t->run();

################################################################################

my $response = ApiManager::http_get($NginxPort,'/shelves?key=this-is-key');

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
$t->stop_daemons();

my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');

my @requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @requests, 2, 'Service control received 2 requests.');

my $r = shift @requests;
is($r->{verb}, 'POST', 'Service control :check was post');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', ':check was called');
is($r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}", 'Host header was set for :check');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', ':check Content-Type was protocol buffer');

my $check_body = ServiceControl::convert_proto($r->{body}, 'check_request', 'json');
my $expected_check_body = {
  'serviceName' => 'endpoints-test.cloudendpointsapis.com',
  'serviceConfigId' => '2016-08-25r1',
  'operation' => {
     'consumerId' => 'api_key:this-is-key',
     'operationName' => 'test.Bookstore.ListShelves',
     'labels' => {
        'servicecontrol.googleapis.com/caller_ip' => '127.0.0.1',
        'servicecontrol.googleapis.com/service_agent' => ServiceControl::service_agent(),
        'servicecontrol.googleapis.com/user_agent' => 'ESP',
     }
  }
};
ok(ServiceControl::compare_json($check_body, $expected_check_body), 'Check body is received.');

my $r = shift @requests;
is($r->{verb}, 'POST', 'Service control :report was post');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:report', ':report was called');
is($r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}", 'Host header was set for :report');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', ':report Content-Type was protocol buffer');

my $report_body = ServiceControl::convert_proto($r->{body}, 'report_request', 'json');
my $expected_report_body = {
 'serviceName' => 'endpoints-test.cloudendpointsapis.com',
 'serviceConfigId' => '2016-08-25r1',
 'operations' => [
  {
   'operationName' => 'test.Bookstore.ListShelves',
   'consumerId' => 'api_key:this-is-key',
   'labels' => {
    'cloud.googleapis.com/location' => 'us-central1',
    'servicecontrol.googleapis.com/service_agent' => ServiceControl::service_agent(),
    'servicecontrol.googleapis.com/platform' => 'unknown',
    'servicecontrol.googleapis.com/user_agent' => 'ESP',
    'serviceruntime.googleapis.com/api_method' => 'test.Bookstore.ListShelves',
    'serviceruntime.googleapis.com/api_version' => 'v1'
   },
   'logEntries' => [
    {
     'structPayload' => {
      'api_key' => 'this-is-key',
      'api_name' => 'test.Bookstore',
      'api_version'=> 'v1',
      'url' => '/shelves?key=this-is-key',
      'api_method' => 'test.Bookstore.ListShelves',
      'location' => 'us-central1',
      'log_message' => 'Method: test.Bookstore.ListShelves',
      'http_response_code' => 200,
      'request_size_in_bytes' => 55,
      'response_size_in_bytes' =>  90,
      'http_method' => 'GET'
     },
     'name' => 'endpoints_log',
     'severity' => 'INFO'
    }
   ]
  }
 ]
};
use Data::Dumper;
print Dumper $report_body;
ok(ServiceControl::compare_json($report_body, $expected_report_body), 'Report body is received.');

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

  $server->on_sub('GET', '/shelves', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });
  $server->run();
}

################################################################################
