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
my $PubkeyPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(10);

# log_jwt_payload must be a primitive field.
# object and array are not supported, which will not be logged.
$t->write_file('sc_override.pb.txt', <<"EOF");
service_control_config {
  log_jwt_payload: "exp"
  log_jwt_payload: "google"
  log_jwt_payload: "google.compute_engine.project_id."
  log_jwt_payload: "google.project_number."
  log_jwt_payload: "google.google_bool"
  log_jwt_payload: "google.not_existed"
  log_jwt_payload: "foo.foo_list"
  log_jwt_payload: "foo.foo_bool"
  log_jwt_payload: "google.compute_engine.not_existed"
  log_jwt_payload: "aud."
  log_jwt_payload: "not_existed."
}
EOF

my $config = ApiManager::get_bookstore_service_config_allow_some_unregistered .
             ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF";
producer_project_id: "endpoints-test"
authentication {
  providers [
    {
      id: "test_auth"
      issuer: "es256-issuer"
      jwks_uri: "http://127.0.0.1:${PubkeyPort}/key"
    }
  ]
  rules {
    selector: "ListShelves"
    requirements [
      {
        provider_id: "test_auth"
        audiences: "ok_audience_1"
      }
    ]
  }
}
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
  server {
    listen 127.0.0.1:${NginxPort};
    server_name localhost;
    location / {
      endpoints {
        api service.pb.txt;
        server_config sc_override.pb.txt;
        %%TEST_CONFIG%%
        on;
      }
      proxy_pass http://127.0.0.1:${BackendPort};
    }
  }
}
EOF

my $pubkeys = <<'EOF';
{
 "keys": [
  {
    "e":"AQAB",
    "kid":"DHFbpoIUqrY8t2zpA2qXfCmr5VO5ZEr4RzHU_-envvQ",
    "kty":"RSA",
    "n":"xAE7eB6qugXyCAG3yhh7pkDkT65pHymX-P7KfIupjf59vsdo91bSP9C8H07pSAGQO1MV_xFj9VswgsCg4R6otmg5PV2He95lZdHtOcU5DXIg_pbhLdKXbi66GlVeK6ABZOUW3WYtnNHD-91gVuoeJT_DwtGGcp4ignkgXfkiEm4sw-4sfb4qdt5oLbyVpmW6x9cfa7vs2WTfURiCrBoUqgBo_-4WTiULmmHSGZHOjzwa8WtrtOQGsAFjIbno85jp6MnGGGZPYZbDAa_b3y5u-YpW7ypZrvD8BgtKVjgtQgZhLAGezMt0ua3DRrWnKqTZ0BJ_EyxOGuHJrLsn00fnMQ"
  }
 ]
}
EOF


# es256_token is issued by "es256-issuer".
# Generated with payloads:
# {
#   "aud": "ok_audience_1",
#   "exp": 4703162488,
#  "foo": {
#    "foo_list": [
#      true,
#      false
#    ],
#    "foo_bool": true
#  },
#   "google": {
#     "compute_engine": {
#       "project_id": "cloudendpoint_testing",
#       "zone": "us_west1_a",
#   }
#   "project_number": 12345,
#   "google_bool": false
#  },
#  "iat": 1549412881,
#  "iss": "es256-issuer",
#  "sub": "es256-issuer"
#}

my $es256_token = "eyJhbGciOiJSUzI1NiIsImtpZCI6IkRIRmJwb0lVcXJZOHQyenBBMnFYZk".
"NtcjVWTzVaRXI0UnpIVV8tZW52dlEiLCJ0eXAiOiJKV1QifQ.eyJhdWQiOiJva19hdWRpZW5jZV8".
"xIiwiZXhwIjo0NzAzMTYyNDg4LCJmb28iOnsiZm9vX2Jvb2wiOnRydWUsImZvb19saXN0IjpbdHJ".
"1ZSxmYWxzZV19LCJnb29nbGUiOnsiY29tcHV0ZV9lbmdpbmUiOnsicHJvamVjdF9pZCI6ImNsb3V".
"kZW5kcG9pbnRfdGVzdGluZyIsInpvbmUiOiJ1c193ZXN0MV9hIn0sImdvb2dsZV9ib29sIjpmYWx".
"zZSwicHJvamVjdF9udW1iZXIiOjEyMzQ1fSwiaWF0IjoxNTQ5NTYyNDg4LCJpc3MiOiJlczI1Ni1".
"pc3N1ZXIiLCJzdWIiOiJlczI1Ni1pc3N1ZXIifQ.SnQ66iwlS80VFvtL-8jeEyqtaxaqW0CgN0W4".
"DoJ5imwatHm1If_ty7EbjZUf-ilUawxD_G-xV6_YJ59JX-C6X3SD_yYYrhJZac1V99awCxG3LxTp".
"ziiOLzTOY28-xayHNwKLQT_qwM3RoJ4eFO1jOzcwxZdvGiyBBuoaht0cygqqFecfxjaBHtGwfyxQ".
"cR__FNFxZ2JGwL9PK4ytttFFOey1FOIyDM3kd3O2NwMAb8zfI2vPwKizEEYnWqgsfNkzckp02W4s".
"01IgOPc5s2XMUjnWoSk_is1Hc527jvIOQhnSDZyHqt9QfsDKdNvZ0qj7E_3p2rbaaTiInogDsvj0".
"aA";

my $report_done = 'report_done';

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);

$t->run_daemon(\&key, $t, $PubkeyPort, $pubkeys, 'key.log');

is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${PubkeyPort}"), 1, 'Pubkey socket ready.');

$t->run();

################################################################################
# RS256-signed jwt token is passed in "Authorization: Bearer" header.

my $response = ApiManager::http($NginxPort,<<"EOF");
GET /shelves?key=this-is-an-api-key HTTP/1.0
Host: localhost
Authorization: Bearer $es256_token

EOF

is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
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

my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
# Check was not called since no api_key and allow_unregistered_calls.
is(scalar @servicecontrol_requests, 2, 'Service control was called once');

# :report
my $r = shift @servicecontrol_requests;
$r = shift @servicecontrol_requests;
like($r->{uri}, qr/:report$/, 'The call was a :report');

my $report_json = decode_json(ServiceControl::convert_proto($r->{body}, 'report_request', 'json'));
my @operations = @{$report_json->{operations}};
is(scalar @operations, 1, 'There are 1 report operations total');

my $log = $report_json->{operations}[0]->{logEntries}[0]->{structPayload};
is($log->{jwt_payloads}, 'exp=4703162488;project_id=cloudendpoint_testing;project_number=12345;google_bool=false;foo_bool=true;aud=ok_audience_1;', 'log message includes configured jwt payloads');

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

{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF
  });

  $server->run();
}

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

sub key {
  my ($t, $port, $keys, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('GET', '/key', sub {
    my ($headers, $body, $client) = @_;
    print $client <<"EOF";
HTTP/1.1 200 OK
Connection: close

$keys
EOF
  });

  $server->run();
}

