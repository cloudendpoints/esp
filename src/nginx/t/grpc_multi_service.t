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

# Port assignment
my $Http2NginxPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $GrpcBackend1Port = ApiManager::pick_port();
my $GrpcBackend2Port = ApiManager::pick_port();
my $HttpBackendPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(7);

$t->write_file(
    'interop_service.pb.txt',
    ApiManager::get_grpc_interop_service_config . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

$t->write_file('test_service.pb.txt',
        ApiManager::get_grpc_test_service_config($GrpcBackend2Port) .
        ApiManager::read_test_file('testdata/logs_metrics.pb.txt') . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
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
  server {
    listen 127.0.0.1:${Http2NginxPort} http2;
    server_name localhost;
    location /grpc.testing.TestService {
      endpoints {
        api interop_service.pb.txt;
        on;
      }
      grpc_pass 127.0.0.1:${GrpcBackend1Port};
    }
    location /test.grpc.Test {
      endpoints {
        api test_service.pb.txt;
        on;
      }
      grpc_pass 127.0.0.1:${GrpcBackend2Port};
    }
  }
}
EOF

$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'servicecontrol.log');
$t->run_daemon(\&ApiManager::grpc_interop_server, $t, "${GrpcBackend1Port}");
$t->run_daemon(\&ApiManager::grpc_test_server, $t, "127.0.0.1:${GrpcBackend2Port}");
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${GrpcBackend1Port}"), 1, 'GRPC test server socket ready.');
is($t->waitforsocket("127.0.0.1:${GrpcBackend2Port}"), 1, 'GRPC test server socket ready.');
$t->run();
is($t->waitforsocket("127.0.0.1:${Http2NginxPort}"), 1, 'Nginx socket ready.');

################################################################################
my @test_cases = (
    'empty_unary',
    'large_unary',
);

foreach my $case (@test_cases) {
  my $result = &ApiManager::run_grpc_interop_test($t, $Http2NginxPort,
      $case, '--global_metadata', 'x-api-key:api-key');
  is($result, 0, "${case} test completed as expected.");
}

my $test_results = &ApiManager::run_grpc_test($t, <<"EOF");
server_addr: "127.0.0.1:${Http2NginxPort}"
plans {
  echo {
    call_config {
      api_key: "this-is-an-api-key"
    }
    request {
      text: "Hello, world!"
    }
  }
}
EOF

$t->stop_daemons();

my $test_results_expected = <<'EOF';
results {
  echo {
    text: "Hello, world!"
  }
}
EOF
is($test_results, $test_results_expected, 'Client tests completed as expected.');


################################################################################

sub service_control {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on_sub('POST', '/v1/services/endpoints-grpc-interop.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/endpoints-grpc-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->run();
}

################################################################################
