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

################################################################################

# Port assignments
my $NginxPort = 8080;
my $ServiceControlPort = 8081;
my $GrpcBackendPort = 8082;
my $GrpcFallbackPort = 8085;

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(7);

$t->write_file('service.pb.txt', ApiManager::get_grpc_test_service_config . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
system_parameters {
  rules {
    selector: "test.grpc.Test.Echo"
    parameters {
      name: "api_key"
      http_header: "x-api-key"
    }
  }
  rules {
    selector: "test.grpc.Test.EchoStream"
    parameters {
      name: "api_key"
      http_header: "x-api-key"
    }
  }
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
    listen 127.0.0.1:${NginxPort} http2;

    # The max concurrent streams must be larger than parallel_limit of test plans,
    # and must be smaller than test_count. So it fails when nginx fails to close
    # streams for GRPC requests.
    http2_max_concurrent_streams 20;

    server_name localhost;
    location / {
      endpoints {
        api service.pb.txt;
        on;
      }
      grpc_pass {
        proxy_pass http://127.0.0.1:${ServiceControlPort}/;
      }
      grpc_backend_address_fallback 127.0.0.2:${GrpcFallbackPort};
    }
  }
}
EOF

$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'requests.log');
$t->run_daemon(\&ApiManager::grpc_test_server, $t, "127.0.0.1:${GrpcBackendPort}");
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
is($t->waitforsocket("127.0.0.1:${GrpcBackendPort}"), 1, 'GRPC test server socket ready.');
$t->run();
is($t->waitforsocket("127.0.0.1:${NginxPort}"), 1, 'Nginx socket ready.');

################################################################################
my $test_results = &ApiManager::run_grpc_test($t, <<"EOF");
server_addr: "127.0.0.1:${NginxPort}"
plans {
  parallel {
    test_count: 100
    parallel_limit: 10
    subtests {
      weight: 1
      echo {
        request {
          text: "Hello, world!"
        }
        call_config {
          api_key: "this-is-an-api-key"
        }
      }
    }
    subtests {
      weight: 1
      echo_stream {
        request {
          text: "Hello, world!"
        }
        call_config {
          api_key: "this-is-an-api-key"
        }
        count: 100
      }
    }
    subtests {
      weight: 1
      echo {
        request {
          text: "Hello, world!"
        }
      }
    }
    subtests {
      weight: 1
      echo_stream {
        request {
          text: "Hello, world!"
        }
        count: 100
      }
    }
  }
}
EOF

$t->stop_daemons();

my $test_results_expected = <<'EOF';
results {
  parallel {
    total_time_micros: \d+
    stats {
      succeeded_count: (\d+)
      mean_latency_micros: \d+
      stddev_latency_micros: \d+
    }
    stats {
      succeeded_count: (\d+)
      mean_latency_micros: \d+
      stddev_latency_micros: \d+
    }
    stats {
      failed_count: (\d+)
      mean_latency_micros: \d+
      stddev_latency_micros: \d+
    }
    stats {
      failed_count: (\d+)
      mean_latency_micros: \d+
      stddev_latency_micros: \d+
    }
  }
}
EOF
like($test_results, qr/$test_results_expected/m, 'Client tests completed as expected.');
if ($test_results =~ /$test_results_expected/) {
  my $succeeded = $1 + $2;
  my $failed = $3 + $4;
  cmp_ok($succeeded, '>=', 40, 'At least 40 successful requests');
  cmp_ok($failed, '>=', 50, 'At least 50 failed requests');
  is($succeeded + $failed, 100, 'Total requests');
} else {
  fail('Able to pull out test results');
}

################################################################################

sub service_control {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";

  $server->on_sub('POST', '/v1/services/endpoints-grpc-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/endpoints-grpc-test.cloudendpointsapis.com:report', sub {
    my ($headers, $body, $client) = @_;
    print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
  });

  $server->run();
}

################################################################################
