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
use src::nginx::t::ServiceControl;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework
use JSON::PP;

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();
my $GrpcServerPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(20);

$t->write_file('service.json',
  ApiManager::get_transcoding_test_service_config(
    'endpoints-transcoding-test.cloudendpointsapis.com',
    "http://127.0.0.1:${ServiceControlPort}"));

$t->write_file('server_config.pb.txt', ApiManager::disable_service_control_cache);

$t->write_file_expand('nginx.conf', <<EOF);
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
        server_config server_config.pb.txt;
        on;
      }
      grpc_pass 127.0.0.1:${GrpcServerPort};
    }
  }
}
EOF

my $report_done = 'report_done';

$t->run_daemon(\&service_control, $t, $ServiceControlPort, 'servicecontrol.log', $report_done);
ApiManager::run_transcoding_test_server($t, 'server.log', "127.0.0.1:${GrpcServerPort}");

is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, "Service control socket ready.");
is($t->waitforsocket("127.0.0.1:${GrpcServerPort}"), 1, "GRPC test server socket ready.");
$t->run();
is($t->waitforsocket("127.0.0.1:${NginxPort}"), 1, "Nginx socket ready.");

################################################################################

# 1. Binding shelf=1 in ListBooksRequest
#    HTTP template:
#      GET /shelves/{shelf}/books
my $response = ApiManager::http_get($NginxPort,'/shelves/1/books?key=api-key');
ok(ApiManager::verify_http_json_response(
    $response,
    { 'books' => [ {'id' => '1', 'author' => 'Neal Stephenson', 'title' => 'Readme'} ] }),
    'Got initial books for shelf 1');

# 2. Binding shelf=2 in ListBooksRequest
#    HTTP template:
#      GET /shelves/{shelf}/books
$response = ApiManager::http_get($NginxPort,'/shelves/2/books?key=api-key');
ok(ApiManager::verify_http_json_response(
    $response,
    { 'books' => [ {'id' => '2', 'author' => 'George R.R. Martin', 'title' => 'A Game of Thrones'} ] }),
    'Got initial books for shelf 2');

# 3. Binding shelf=1 and book=<post body> in CreateBookRequest
#    HTTP template:
#      POST /shelves/{shelf}/books
#      body: book
$response = ApiManager::http($NginxPort,<<EOF);
POST /shelves/1/books?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 53

{"author" : "Leo Tolstoy", "title" : "War and Peace"}
EOF
ok(ApiManager::verify_http_json_response(
    $response, {'id' => '3', 'author' => 'Leo Tolstoy', 'title' => 'War and Peace'}),
    'Got the new book');

# 4. Binding shelf=1, book.id=4, book.author="Mark Twain" and book.title=<post body>
#    in CreateBookRequest.
#    HTTP template:
#      POST /shelves/{shelf}/books/{book.id}/{book.author}
#      body: book.title
$response = ApiManager::http($NginxPort,<<EOF);
POST /shelves/1/books/4/Mark%20Twain?key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 36

"The Adventures of Huckleberry Finn"
EOF
ok(ApiManager::verify_http_json_response(
    $response, {'id' => '4', 'author' => 'Mark Twain', 'title' => 'The Adventures of Huckleberry Finn'}),
    'Got the new book');

# 5. Binding shelf=1 in ListBooksRequest
#    HTTP template:
#      GET /shelves/{shelf}/books
$response = ApiManager::http_get($NginxPort,'/shelves/1/books?key=api-key');
ok(ApiManager::verify_http_json_response(
    $response, { 'books' => [
        {'id' => '1', 'author' => 'Neal Stephenson', 'title' => 'Readme'},
        {'id' => '3', 'author' => 'Leo Tolstoy', 'title' => 'War and Peace'},
        {'id' => '4', 'author' => 'Mark Twain', 'title' => 'The Adventures of Huckleberry Finn'},
      ] }),
    'Got all books for shelf 1');

# 6. Binding shelf=1 and book=3 in DeleteBookRequest
#    HTTP template:
#      DELETE /shelves/{shelf}/books/{book}
$response = ApiManager::http($NginxPort,<<EOF);
DELETE /shelves/1/books/3?key=api-key HTTP/1.0
HOST: 127.0.0.1:${NginxPort}

EOF
ok(ApiManager::verify_http_json_response($response, {}), 'Got empty resonse for delete');

# 7. Binding shelf=1 in ListBooksRequest
#    HTTP template:
#      GET /shelves/{shelf}/books
$response = ApiManager::http_get($NginxPort,'/shelves/1/books?key=api-key');
ok(ApiManager::verify_http_json_response(
    $response, { 'books' => [
        {'id' => '1', 'author' => 'Neal Stephenson', 'title' => 'Readme'},
        {'id' => '4', 'author' => 'Mark Twain', 'title' => 'The Adventures of Huckleberry Finn'},
      ] }),
    'Got final list of books for shelf 1');


# Wait for the service control report
is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
$t->stop_daemons();

################################################################################

# Read and verify backend logs
my $server_output = $t->read_file('server.log');
my @translated_requests = split /\r\n\r\n/, $server_output;

# Check the translated requests
is (scalar @translated_requests, 7, 'The server got the expected requests');

# 1. ListBooksRequest with shelf=1
ok(ApiManager::compare_json($translated_requests[0], {'shelf'=>'1'}));

# 2. ListBooksRequest with shelf=2
ok(ApiManager::compare_json($translated_requests[1], {'shelf'=>'2'}));

# 3. CreateBookRequest with shelf=1 and book={"author" : "Leo Tolstoy", "title" : "War and Peace"}
ok(ApiManager::compare_json(
    $translated_requests[2], {'shelf'=>'1', book => {'author' => 'Leo Tolstoy', 'title' => 'War and Peace'}} ));

# 4. CreateBookRequest with shelf=1 and book={"id" : "4", "author" : "Mark Twain",
#                                             "title" : "The Adventures of Huckleberry Finn"}
ok(ApiManager::compare_json(
    $translated_requests[3],
    {'shelf'=>'1', book => {'id' => '4', 'author' => 'Mark Twain',
                            'title' => 'The Adventures of Huckleberry Finn'}} ));

# 5. ListBooksRequest with shelf=1
ok(ApiManager::compare_json($translated_requests[4], {'shelf'=>'1'}));

# 6. DeleteBookRequest with shelf=1 and book=3
ok(ApiManager::compare_json($translated_requests[5], {'shelf'=>'1', 'book'=>'3'}));

# 7. ListBooksRequest with shelf=1
ok(ApiManager::compare_json($translated_requests[6], {'shelf'=>'1'}));

# Expect 14 service control calls for 7 requests.
my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 14, 'Service control was called 14 times');

################################################################################

sub service_control {
  my ($t, $port, $file, $done) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";

  local $SIG{PIPE} = 'IGNORE';
  my $request_count = 0;

  $server->on_sub('POST', '/v1/services/endpoints-transcoding-test.cloudendpointsapis.com:check', sub {
    my ($headers, $body, $client) = @_;
    print $client <<EOF;
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
  });

  $server->on_sub('POST', '/v1/services/endpoints-transcoding-test.cloudendpointsapis.com:report', sub {
    my ($headers, $body, $client) = @_;
    print $client <<EOF;
HTTP/1.1 200 OK
Content-Type: application/json
Connection: close

EOF
    $request_count++;
    if ($request_count == 7) {
      $t->write_file($done, ":report done");
    }
  });

  $server->run();
}

################################################################################
