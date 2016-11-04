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

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(26);
my $total_requests = 10;

$t->write_file('service.pb.txt',
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
        api service.pb.txt;
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

# 1. Binding theme='Classics' in CreateShelfRequest
#    Using the following HTTP template:
#      POST /shelves
#      body: shelf
my $response = ApiManager::http($NginxPort,<<EOF);
POST /shelves?key=api-key&shelf.theme=Classics HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 2

{}
EOF
ok(ApiManager::verify_http_json_response(
    $response, {'id' => '3', 'theme' => 'Classics'}),
    'Got the new "Classics" shelf');

# 2. Binding theme='Children' and id='999' in CreateShelfRequest
$response = ApiManager::http($NginxPort,<<EOF);
POST /shelves?shelf.id=999&shelf.theme=Children&key=api-key HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 2

{}
EOF
ok(ApiManager::verify_http_json_response(
    $response, {'id' => '999', 'theme' => 'Children'}),
    'Got the new "Children" shelf');

# 3. Binding shelf=3, book=<post body> and book.title=Readme in CreateBookRequest
#    Using the following HTTP template:
#      POST /shelves/{shelf}/books
#      body: book
$response = ApiManager::http($NginxPort,<<EOF);
POST /shelves/3/books?key=api-key&book.title=Readme HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 30

{"author" : "Neal Stephenson"}
EOF
ok(ApiManager::verify_http_json_response(
    $response, {'id' => '3', 'author' => 'Neal Stephenson', 'title' => 'Readme'}),
    'Got the new "Readme" book');

# 4. Testing URL decoding
$response = ApiManager::http($NginxPort,<<EOF);
POST /shelves/1/books?key=api-key&book.title=War%20%26%20Peace HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 26

{"author" : "Leo Tolstoy"}
EOF
ok(ApiManager::verify_http_json_response(
    $response, {'id' => '4', 'author' => 'Leo Tolstoy', 'title' => 'War & Peace'}),
    'Got the new "War & Peace" book');

# 5. Binding all book fields through query params
$response = ApiManager::http($NginxPort,<<EOF);
POST /shelves/3/books?key=api-key&book.id=99&book.author=Leo%20Tolstoy&book.title=Anna%20Karenina HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: 2

{}
EOF
ok(ApiManager::verify_http_json_response(
    $response, {'id' => '99', 'author' => 'Leo Tolstoy', 'title' => 'Anna Karenina'}),
    'Got the new "Anna Karenina" book');

# 6. Binding shelf.id=3 in QueryShelvesRequest
#    Using the following HTTP template:
#      GET /shelves/query
$response = ApiManager::http_get($NginxPort,'/query/shelves?shelf.id=3&key=api-key');
ok(ApiManager::verify_http_json_response(
    $response, {'shelves' =>  [{'id' => '3', 'theme' => 'Classics'}]}),
    'Got the "Classics" shelf');

# 7. Binding shelf.theme=Children in QueryShelvesRequest
$response = ApiManager::http_get($NginxPort,'/query/shelves?shelf.theme=Children&key=api-key');
ok(ApiManager::verify_http_json_response(
    $response, {'shelves' =>  [{'id' => '999', 'theme' => 'Children'}]}),
    'Got the "Children" shelf');

# 8. Binding book.author=Leo%20Tolstoy in QueryBookRequest
#    Using the following HTTP template:
#      GET /shelves/query
$response = ApiManager::http_get($NginxPort,'/query/books?book.author=Leo%20Tolstoy&key=api-key');
ok(ApiManager::verify_http_json_response(
    $response, {'books' =>  [
            {'id' => '4', 'author' => 'Leo Tolstoy', 'title' => 'War & Peace'},
            {'id' => '99', 'author' => 'Leo Tolstoy', 'title' => 'Anna Karenina'} ]}),
    'Got the Leo Tolstoy books');

# 9. Binding shelf=3 and book.author=Leo%20Tolstoy in QueryBookRequest
$response = ApiManager::http_get($NginxPort,'/query/books?shelf=3&book.author=Leo%20Tolstoy&key=api-key');
ok(ApiManager::verify_http_json_response(
    $response, {'books' =>  [
            {'id' => '99', 'author' => 'Leo Tolstoy', 'title' => 'Anna Karenina'} ]}),
    'Got the Leo Tolstoy books on shelf 3');

# 10. Binding shelf=3, book=<post body> and the repeated field book.quote with
#     two values ("Winter is coming" and "Hold the door") in CreateBookRequest.
#     These values should be added to the repeated field in addition to what is
#     translated in the body.
#    Using the following HTTP template:
#      POST /shelves/{shelf}/books
#      body: book
my $request = <<'EOF';
{
  "id" : "1000",
  "author" : "George R.R. Martin",
  "title": "A Game of Thrones",
  "quotes" : [
    "A girl has no name",
    "A very small man can cast a very large shadow"
  ]
}
EOF

my $request_size = length($request);

$response = ApiManager::http($NginxPort,<<EOF);
POST /shelves/3/books?key=api-key&book.quotes=Winter%20is%20coming&book.quotes=Hold%20the%20door HTTP/1.0
Host: 127.0.0.1:${NginxPort}
Content-Type: application/json
Content-Length: $request_size

$request
EOF
ok(ApiManager::verify_http_json_response(
    $response,
    {
      'id' => '1000',
      'author' => 'George R.R. Martin',
      'title' => 'A Game of Thrones',
      'quotes' => [
        'A girl has no name',
        'A very small man can cast a very large shadow',
        'Winter is coming',
        'Hold the door'
      ]
    }), 'Got the new "Game of Thrones" book');


# Wait for the service control report
is($t->waitforfile("$t->{_testdir}/${report_done}"), 1, 'Report body file ready.');
$t->stop_daemons();

################################################################################

# Read and verify backend logs
my $server_output = $t->read_file('server.log');
my @translated_requests = split /\r\n\r\n/, $server_output;

# Check the translated requests
is (scalar @translated_requests, $total_requests, 'The server got the expected requests');

# 1. CreateShelfRequest with shelf={"theme" : "Classics"}
ok(ApiManager::compare_json(
    $translated_requests[0], {shelf => {'theme'=>'Classics'}} ),
    'The translated "Classics" shelf request is as expected');

# 2. CreateShelfRequest with shelf={"id" : "999", "theme" : "Children"}
ok(ApiManager::compare_json(
    $translated_requests[1], {shelf => {'id'=>'999', 'theme'=>'Children'}} ),
    'The translated "Children" shelf request is as expected');

# 3. CreateBookRequest with shelf=3 and book={"author" : "Neal Stephenson", "title" : "Readme"}
ok(ApiManager::compare_json(
    $translated_requests[2], {'shelf'=>'3', book => {'author'=>'Neal Stephenson', 'title'=>'Readme'}} ),
    'The translated "Readme" book request is as expected');

# 4. CreateBookRequest with shelf=1 and book={"author" : "Leo Tolstoy", "title" : "War & Peace"}
ok(ApiManager::compare_json(
    $translated_requests[3],
    {'shelf'=>'1', book => {'author'=>'Leo Tolstoy', 'title'=>'War & Peace'}} ),
    'The translated "War & Peace" book request is as expected');

# 5. CreateBookRequest with shelf=3 and book={"id" : 99, "author" : "Leo Tolstoy", "title" : "Anna Karenina"}
ok(ApiManager::compare_json(
    $translated_requests[4],
    {'shelf'=>'3', book => {id=>'99', 'author'=>'Leo Tolstoy', 'title'=>'Anna Karenina'}} ),
    'The translated "Anna Karenina" book request is as expected');

# 6. QueryShelvesRequest with shelf={"id" : "3"}
ok(ApiManager::compare_json(
    $translated_requests[5], {shelf => {'id'=>'3'}} ),
    'The translated Query shelf.id=3 request is as expected');

# 7. QueryShelvesRequest with shelf={"theme" : "Children"}
ok(ApiManager::compare_json(
    $translated_requests[6], {shelf => {'theme'=>'Children'}} ),
    'The translated Query shelf.theme=Children request is as expected');

# 8. QueryBooksRequest with book={"author" : "Leo Tolstoy"}
ok(ApiManager::compare_json(
    $translated_requests[7], {book => {'author'=>'Leo Tolstoy'}} ),
    'The translated Query book.author=Leo%20Tolstoy request is as expected');

# 9. QueryBooksRequest with shelf=3 and book={"author" : "Leo Tolstoy"}
ok(ApiManager::compare_json(
    $translated_requests[8], {shelf => '3', book => {'author'=>'Leo Tolstoy'}} ),
    'The translated Query shelf=3 and book.author=Leo%20Tolstoy request is as expected');

# 10. CreateBookRequest with shelf=3 and book with 4 quotes
ok(ApiManager::compare_json(
    $translated_requests[9],
    {
      shelf => '3',
      book =>
      {
        'id'=>'1000',
        'author' => 'George R.R. Martin',
        'title' => 'A Game of Thrones',
        'quotes' => [
          'A girl has no name',
          'A very small man can cast a very large shadow',
          'Winter is coming',
          'Hold the door'
        ]
      }
    } ),
    'The translated CreateBookRequest with 4 quotes is as expected');

# Expect 2*$total_requests service control calls for $total_requests.
my @servicecontrol_requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @servicecontrol_requests, 2*$total_requests, 'Service control was called as expected');

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
    if ($request_count == $total_requests) {
      $t->write_file($done, ":report done");
    }
  });

  $server->run();
}

################################################################################
