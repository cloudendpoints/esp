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
my $GrpcBackendPort = ApiManager::pick_port();
my $HttpBackendPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(6);

$t->write_file(
    'service.pb.txt',
    ApiManager::get_grpc_interop_service_config . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

$t->write_file('root.crt', ApiManager::read_test_file('testdata/grpc/cacert.pem'));
my $root_crt_path = $t->testdir() . '/root.crt';
$t->write_file('server_config.pb.txt', <<"EOF");
grpc_backend_ssl_credentials {
  use_ssl: true
  root_certs_file: "${root_crt_path}"
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
    location / {
      endpoints {
        api service.pb.txt;
        server_config server_config.pb.txt;
        on;
      }
      grpc_pass 127.0.0.1:${GrpcBackendPort};
    }
  }
}
EOF

sub run_all_daemons {
  # Start all daemons: groc interop server with ssl and service_control
  $t->write_file('test.key', ApiManager::read_test_file('testdata/grpc/serverkey.pem'));
  $t->write_file('test.crt', ApiManager::read_test_file('testdata/grpc/servercert.pem'));
  my @ssl_args = (
    '--use_tls',
    '--tls_key_file', $t->testdir() . '/test.key',
    '--tls_cert_file', $t->testdir() . '/test.crt',
  );

  $t->run_daemon(\&service_control, $t, $ServiceControlPort, 'servicecontrol.log');
  $t->run_daemon(\&ApiManager::grpc_interop_server, $t, "${GrpcBackendPort}", @ssl_args);
  $t->run();
  is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
  is($t->waitforsocket("127.0.0.1:${GrpcBackendPort}"), 1, 'GRPC test server socket ready.');
  is($t->waitforsocket("127.0.0.1:${Http2NginxPort}"), 1, 'Nginx socket ready.');
}

################################################################################

# 1 test is: server is expecting ssl, but client is not,

# empty server_config: not using ssl
$t->write_file('server_config.pb.txt', <<"EOF");
grpc_backend_ssl_credentials {
}
EOF
run_all_daemons();

my $result1 = &ApiManager::run_grpc_interop_test($t, $Http2NginxPort, 'empty_unary',
   '--additional_metadata', 'x-api-key:api-key');
isnt($result1, 0, "test1 expected to be failed, not using ssl");

$t->stop;
$t->stop_daemons();

################################################################################

# 2 test is: server is expecting ssl, but client is ssl, but not root ca

# this test is skipped due a grpc bug: https://github.com/grpc/grpc/issues/18776
# the request will hang if channel has credential problems, such as could not read files.

$t->write_file('server_config.pb.txt', <<"EOF");
grpc_backend_ssl_credentials {
  use_ssl: true
}
EOF
#run_all_daemons();

#my $result2 = &ApiManager::run_grpc_interop_test($t, $Http2NginxPort, 'empty_unary',
#    '--additional_metadata', 'x-api-key:api-key');
#isnt($result2, 0, "test2 expected to be failed: use ssl but without root CA");

#$t->stop;
#$t->stop_daemons();

################################################################################

# 3 test is: server is expecting ssl, but client is ssl, and correct root CA

$t->write_file('root.crt', ApiManager::read_test_file('testdata/grpc/cacert.pem'));
my $root_crt_path = $t->testdir() . '/root.crt';
$t->write_file('server_config.pb.txt', <<"EOF");
grpc_backend_ssl_credentials {
  use_ssl: true
  root_certs_file: "${root_crt_path}"
}
EOF
run_all_daemons();

my $result3 = &ApiManager::run_grpc_interop_test($t, $Http2NginxPort, 'empty_unary',
   '--additional_metadata', 'x-api-key:api-key');
is($result3, 0, "test3 expected to be success");

$t->stop;
$t->stop_daemons();

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

  $server->run();
}

################################################################################
