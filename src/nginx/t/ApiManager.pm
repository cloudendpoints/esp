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
# A shared module for ESP end-to-end tests.
# Sets up a TEST_NGINX_BINARY environment variable for Nginx test framework
# to find ESP build of Nginx.
# Adds Nginx test library (nginx-tests/lib) to the module search path.

package ApiManager;

use strict;
use warnings;

use FindBin;
use JSON::PP;
use Data::Dumper;
use MIME::Base64;

sub repo_root {
  my $testdir = $FindBin::Bin;
  my @path = split('/', $testdir);
  return (join('/', @path[0 .. $#path - 3]), $testdir);
}

BEGIN {
  our ($Root, $TestDir) = repo_root();
  our $Nginx = $Root . "/third_party/nginx/objs/nginx";
  our $TestLib = $Root . "/third_party/nginx-tests/lib";

  if (!defined $ENV{TEST_NGINX_BINARY}) {
    $ENV{TEST_NGINX_BINARY} =  $Nginx;
  }
  if (!defined $ENV{TEST_SRCDIR}) {
    $ENV{TEST_SRCDIR} = $Root;
  }
}

use lib $ApiManager::TestLib;

select STDERR; $| = 1;   # flush stderr immediately
select STDOUT; $| = 1;   # flush stdout immediately

sub write_binary_file {
  my ($name, $content) = @_;
  open F, '>>', $name or die "Can't create $name: $!";
  binmode F;
  print F $content;
  close F;
}

sub compare {
  my ($x, $y, $path, $ignore_keys) = @_;

  my $refx = ref $x;
  my $refy = ref $y;
  if(!$refx && !$refy) { # both are scalars
    unless ($x eq $y) {
      print "$path value doesn't match $x != $y.\n";
      return 0;
    }
  }
  elsif ($refx ne $refy) { # not the same type
    print "$path type doesn't match $refx != $refy.\n";
    return 0;
  }
  elsif ($refx eq 'SCALAR' || $refx eq 'REF') {
    return compare(${$x}, ${$y}, $path, $ignore_keys);
  }
  elsif ($refx eq 'ARRAY') {
    if ($#{$x} == $#{$y}) { # same length
      my $i = -1;
      for (@$x) {
        $i++;
        return 0 unless compare(
          $x->[$i], $y->[$i], "$path:[$i]", $ignore_keys);
      }
    }
    else {
      print "$path array size doesn't match: $#{$x} != $#{$y}.\n";
      return 0;
    }
  }
  elsif ($refx eq 'HASH') {
    my @diff = grep { !exists $ignore_keys->{$_} && !exists $y->{$_} } keys %$x;
    if (@diff) {
      print "$path has following extra keys:\n";
      for (@diff) {
        print "$_: $x->{$_}\n";
      }
      return 0;
    }
    for (keys %$y) {
      unless(exists($x->{$_})) {
        print "$path key $_ doesn't exist.\n";
        return 0;
      }
      return 0 unless compare($x->{$_}, $y->{$_}, "$path:$_", $ignore_keys);
    }
  } else {
    print "$path: Not supported type: $refx\n";
    return 0;
  }
  return 1;
}

sub compare_json {
  my ($json, $expected, $random_metrics) = @_;
  my $json_obj = decode_json($json);

  print Dumper $json_obj if $ENV{TEST_NGINX_VERBOSE};
  return compare($json_obj, $expected, "", {});
}

sub compare_json_with_random_metrics {
  my ($json, $expected, @random_metrics) = @_;
  my $json_obj = decode_json($json);

  # A list of metrics with non-deterministic values.
  my %random_metric_map = map { $_ => 1 } @random_metrics;

  # Check and remove the above random metrics before making the comparison.
  my $matched_random_metric_count = 0;
  if (not exists $json_obj->{operations}) {
    return 0;
  }

  my $operation = $json_obj->{operations}->[0];
  if (not exists $operation->{metricValueSets}) {
    return 0;
  }

  my @metric_value_sets;
  foreach my $metric (@{$operation->{metricValueSets}}) {
    if (exists($random_metric_map{$metric->{metricName}})) {
      $matched_random_metric_count += 1;
    } else {
      push @metric_value_sets, $metric;
    }
  }

  if ($matched_random_metric_count != scalar @random_metrics) {
    return 0;
  }
  $operation->{metricValueSets} = \@metric_value_sets;

  print Dumper $json_obj if $ENV{TEST_NGINX_VERBOSE};
  my %ignore_keys = map { $_ => "1" } qw(
    startTime endTime timestamp operationId);
  return compare($json_obj, $expected, "", \%ignore_keys);
}

sub compare_user_info {
  my ($user_info, $expected) = @_;
  my $json_obj = decode_json(decode_base64($user_info));
  print Dumper $json_obj if $ENV{TEST_NGINX_VERBOSE};
  return compare($json_obj, $expected, "", {});
}

sub read_test_file {
  my ($name) = @_;
  local $/;
  open F, '<', $ApiManager::TestDir . '/' . $name or die "Can't open $name: $!";
  my $content = <F>;
  close F;
  return $content;
}

sub write_file_expand {
  if (!defined $ENV{TEST_CONFIG}) {
    $ENV{TEST_CONFIG} = "";
  }
  my ($t, $name, $content) = @_;
  $content =~ s/%%TEST_CONFIG%%/$ENV{TEST_CONFIG}/gmse;
  $t->write_file_expand($name,  $content);
}

sub get_bookstore_service_config {
  return read_test_file("testdata/bookstore.pb.txt");
}

sub get_bookstore_service_config_allow_all_http_requests {
    return read_test_file('testdata/bookstore_allow_all_http_requests.pb.txt');
}

sub get_bookstore_service_config_allow_unregistered {
  return get_bookstore_service_config .
         read_test_file("testdata/usage_fragment.pb.txt");
}

sub get_echo_service_config {
  return read_test_file("testdata/echo_service.pb.txt");
}

sub get_grpc_test_service_config {
  return read_test_file("testdata/grpc_test_service_config.pb.txt");
}

sub get_metadata_response_body {
  return <<EOF;
{
  "instance": {
    "attributes": {
      "gae_app_container": "app",
      "gae_app_fullname": "esp-test-app_20150921t180445-387321214075436208",
      "gae_backend_instance": "0",
      "gae_backend_minor_version": "387321214075436208",
      "gae_backend_name": "default",
      "gae_backend_version": "20150921t180445",
      "gae_project": "esp-test-app",
      "gae_vm_runtime": "custom",
      "gcm-pool": "gae-default-20150921t180445",
      "gcm-replica": "gae-default-20150921t180445-inqp"
    },
    "cpuPlatform": "Intel Ivy Bridge",
    "description": "GAE managed VM for module: default, version: 20150921t180445",
    "hostname": "gae-default-20150921t180445-inqp.c.esp-test-app.internal",
    "id": 3296474103533342935,
    "image": "",
    "machineType": "projects/345623948572/machineTypes/g1-small",
    "maintenanceEvent": "NONE",
    "zone": "projects/345623948572/zones/us-west1-d"
  },
  "project": {
    "numericProjectId": 345623948572,
    "projectId": "esp-test-app"
  }
}
EOF
}

sub grpc_test_server {
  my ($t, @args) = @_;
  my $server = $ENV{TEST_SRCDIR} . '/test/grpc/grpc-test-server';
  exec $server, @args;
}

sub run_grpc_test {
  my ($t, $plans) = @_;
  $t->write_file('test_plans.txt', $plans);
  my $testdir = $t->testdir();
  my $client = $ENV{TEST_SRCDIR} . '/test/grpc/grpc-test-client';
  system "$client < $testdir/test_plans.txt > $testdir/test_results.txt";
  return $t->read_file('test_results.txt');
}

sub run_nginx_with_stderr_redirect {
  my $t = shift;
  my $redirect_file = $t->{_testdir}.'/stderr.log';

  # redirect, fork & run, restore
  open ORIGINAL, ">&", \*STDERR;
  open STDERR, ">", $redirect_file;
  $t->run();
  open STDERR, ">&", \*ORIGINAL;
}

# Runs an HTTP server that returns "404 Not Found" for every request.
sub not_found_server {
  my ($t, $port) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/nop.log')
    or die "Can't create test server socket: $!\n";

  $server->run();
}

# Reads a file which contains a stream of HTTP requests,
# parses out individual requests and returns them in an array.
sub read_http_stream {
  my ($t, $file) = @_;

  my $http = $t->read_file($file);

  # Parse out individual HTTP requests.

  my @requests;

  while ($http ne '') {
    my ($request_headers, $rest) = split /\r\n\r\n/, $http, 2;
    my @header_lines = split /\r\n/, $request_headers;

    my %headers;
    my $verb = '';
    my $uri = '';
    my $path = '';
    my $body = '';

    # Process request line.
    my $request_line = $header_lines[0];
    if ($request_line =~ /^(\S+)\s+(([^? ]+)(\?[^ ]+)?)\s+HTTP/i) {
      $verb = $1;
      $uri = $2;
      $path = $3;
    }

    # Process headers
    foreach my $header (@header_lines[1 .. $#header_lines]) {
      my ($key, $value) = split /\s*:\s*/, $header, 2;
      $headers{lc $key} = $value;
    }

    my $content_length = $headers{'content-length'} || 0;
    if ($content_length > 0) {
      $body = substr $rest, 0, $content_length;
      $rest = substr $rest, $content_length;
    }

    push @requests, {
      'verb' => $verb,
      'path' => $path,
      'uri' => $uri,
      'headers' => \%headers,
      'body' => $body
    };

    $http = $rest;
  }

  return @requests;
}

1;
