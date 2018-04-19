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
# A shared module to generate ServiceControl report Perl object for testing.


use strict;
use warnings;

package ServiceControl;

use src::nginx::t::ApiManager;
use src::nginx::t::Status;
use IPC::Open2;

use Data::Dumper;

my @http1_random_metrics = (
    'serviceruntime.googleapis.com/api/consumer/total_latencies',
    'serviceruntime.googleapis.com/api/producer/total_latencies',
    'serviceruntime.googleapis.com/api/consumer/backend_latencies',
    'serviceruntime.googleapis.com/api/producer/backend_latencies',
    'serviceruntime.googleapis.com/api/consumer/request_overhead_latencies',
    'serviceruntime.googleapis.com/api/producer/request_overhead_latencies',
    'serviceruntime.googleapis.com/api/consumer/streaming_durations',
    'serviceruntime.googleapis.com/api/producer/streaming_durations',
    'serviceruntime.googleapis.com/api/producer/by_consumer/total_latencies',
    'serviceruntime.googleapis.com/api/producer/by_consumer/request_overhead_latencies',
    'serviceruntime.googleapis.com/api/producer/by_consumer/backend_latencies',
);

my @http2_random_metrics = (
    @http1_random_metrics,
    'serviceruntime.googleapis.com/api/consumer/response_sizes',
    'serviceruntime.googleapis.com/api/producer/response_sizes',
    'serviceruntime.googleapis.com/api/consumer/response_bytes',
    'serviceruntime.googleapis.com/api/producer/response_bytes',
    'serviceruntime.googleapis.com/api/producer/by_consumer/request_sizes',
    'serviceruntime.googleapis.com/api/producer/by_consumer/response_sizes',
);

sub gen_metric_int64 {
  my ($name, $value) = @_;
  return {
    'metricName' => $name,
    'metricValues' => [ { 'int64Value' => $value } ]
  };
}

my %time_distribution = (
  buckets => 29,
  growth => 2.0,
  scale => 1e-6
);

my %size_distribution = (
  buckets => 8,
  growth => 10.0,
  scale => 1
);

sub gen_metric_dist {
  my ($options, $name, $value) = @_;

  my @buckets = (0) x ($options->{buckets} + 2);

  my $bucket = 0;
  if ($value >= $options->{scale}) {
    $bucket = 1 + int (log($value / $options->{scale}) / log($options->{growth}));
    if ($bucket > $#buckets) {
      $bucket = $#buckets;
    }
  }
  $buckets[$bucket] = 1;

  my $dist_value = {
    'bucketCounts' => \@buckets,
    'count' => '1',
    'exponentialBuckets' => {
      'numFiniteBuckets' => $options->{buckets},
      'growthFactor' => $options->{growth},
      'scale' => $options->{scale},
    },
  };

  if ($value != 0) {
    $dist_value->{mean} = $value;
    $dist_value->{minimum} = $value;
    $dist_value->{maximum} = $value;
  }

  return {
    'metricName' => $name,
    'metricValues' => [ {
      'distributionValue' => $dist_value
    } ]
  };
}

sub gen_report_labels {
  my $in = shift;

  my $labels = {
    'servicecontrol.googleapis.com/service_agent' => service_agent(),
    'servicecontrol.googleapis.com/user_agent' => 'ESP',
    '/response_code' => $in->{response_code},
    '/status_code' => Status::http_response_code_to_status_code($in->{response_code}),
    '/response_code_class' => (($in->{response_code} / 100) % 10) . "xx",
    '/protocol' => 'http'
  };

  $labels->{'serviceruntime.googleapis.com/api_method'} = $in->{api_method} if exists $in->{api_method};
  $labels->{'/status_code'} = $in->{status_code} if exists $in->{status_code};
  $labels->{'/error_type'} = $in->{error_type} if exists $in->{error_type};
  $labels->{'/protocol'} = $in->{protocol} if exists $in->{protocol};
  $labels->{'servicecontrol.googleapis.com/backend_protocol'} = $in->{backend_protocol} if exists $in->{backend_protocol};
  $labels->{'serviceruntime.googleapis.com/api_version'} = $in->{api_version} if exists $in->{api_version};
  if (exists $in->{location}) {
    $labels->{'cloud.googleapis.com/location'} = $in->{location};
  } else {
    $labels->{'cloud.googleapis.com/location'} = 'us-central1';
  }

  if (exists $in->{platform}) {
    $labels->{'servicecontrol.googleapis.com/platform'} = $in->{platform};
  } else {
    $labels->{'servicecontrol.googleapis.com/platform'} = 'unknown';
  }



  if (exists $in->{api_key}) {
    $labels->{'/credential_id'} = 'apikey:' . $in->{api_key};
  } elsif (exists $in->{jwtAuth}) {
    $labels->{'/credential_id'} = 'jwtauth:' . $in->{jwtAuth};
  }
  return $labels;
}

sub gen_log_entry {
  my $in = shift;

  my $payload = {
    'http_response_code' => $in->{response_code},
  };

  $payload->{api_method} = $in->{api_method} if exists $in->{api_method};
  $payload->{api_name} = $in->{api_name} if exists $in->{api_name};
  $payload->{api_version} = $in->{api_version} if exists $in->{api_version};
  $payload->{producer_project_id} = $in->{producer_project_id} if
    exists $in->{producer_project_id};
  $payload->{api_key} = $in->{api_key} if exists $in->{api_key};
  $payload->{referer} = $in->{referer} if exists $in->{referer};
  $payload->{request_size_in_bytes} = $in->{request_size} if exists $in->{request_size};
  $payload->{response_size_in_bytes} = $in->{response_size} if exists $in->{response_size};
  $payload->{log_message} = $in->{log_message} if exists $in->{log_message};
  $payload->{url} = $in->{url} if exists $in->{url};
  $payload->{http_method} = $in->{http_method} if exists $in->{http_method};
  $payload->{error_cause} = $in->{error_cause} if exists $in->{error_cause};

  my $severity = 'INFO';
  if ($in->{response_code} >= 400) {
    $severity = 'ERROR';
  }

  return {
    'name' => 'endpoints_log',
    'severity' => $severity ,
    'structPayload' => $payload
  };
}

sub gen_report_body {
  my $in = shift;

  my $operation = {};
  if (exists $in->{api_method}) {
    $operation->{operationName} = $in->{api_method};
  } else {
    $operation->{operationName} = '<Unknown Operation Name>';
  }
  if (exists $in->{api_key}) {
    $operation->{consumerId} = 'api_key:' . $in->{api_key};
  }

  my $ll = gen_report_labels($in);
  $operation->{labels} = $ll;

  my @log_entries = (gen_log_entry($in),);
  $operation->{logEntries} = \@log_entries;


  my @metrics = (
    gen_metric_int64(
      'serviceruntime.googleapis.com/api/producer/request_count', 1),

    gen_metric_dist(\%size_distribution,
      'serviceruntime.googleapis.com/api/producer/request_sizes', $in->{request_size}),
    );

  my $send_consumer_metric = (!exists $in->{no_consumer_data}) || (!$in->{no_consumer_data});

  if ($send_consumer_metric) {
    push @metrics, gen_metric_int64(
            'serviceruntime.googleapis.com/api/consumer/request_count', 1);
    push @metrics, gen_metric_dist(\%size_distribution,
        'serviceruntime.googleapis.com/api/consumer/request_sizes', $in->{request_size});
  }

  if (exists $in->{response_size}) {
    push @metrics, gen_metric_dist(\%size_distribution,
      'serviceruntime.googleapis.com/api/producer/response_sizes', $in->{response_size});
    if ($send_consumer_metric)  {
      push @metrics, gen_metric_dist(\%size_distribution,
              'serviceruntime.googleapis.com/api/consumer/response_sizes', $in->{response_size});
    }
  }

  if (exists $in->{streaming_request_message_counts}) {
    push @metrics, gen_metric_dist(\%size_distribution,
            'serviceruntime.googleapis.com/api/producer/streaming_request_message_counts', $in->{streaming_request_message_counts});
    if ($send_consumer_metric)  {
      push @metrics, gen_metric_dist(\%size_distribution,
              'serviceruntime.googleapis.com/api/consumer/streaming_request_message_counts', $in->{streaming_request_message_counts});
    }
  }

  if (exists $in->{streaming_response_message_counts}) {
    push @metrics, gen_metric_dist(\%size_distribution,
            'serviceruntime.googleapis.com/api/producer/streaming_response_message_counts', $in->{streaming_response_message_counts});
    if ($send_consumer_metric)  {
      push @metrics, gen_metric_dist(\%size_distribution,
              'serviceruntime.googleapis.com/api/consumer/streaming_response_message_counts', $in->{streaming_response_message_counts});
    }
  }


  if (exists $in->{request_bytes}) {
    push @metrics, gen_metric_int64(
            'serviceruntime.googleapis.com/api/producer/request_bytes', $in->{request_bytes});
    if ($send_consumer_metric)  {
      push @metrics, gen_metric_int64(
              'serviceruntime.googleapis.com/api/consumer/request_bytes', $in->{request_bytes});
    }
  }

  if (exists $in->{response_bytes}) {
    push @metrics, gen_metric_int64(
            'serviceruntime.googleapis.com/api/producer/response_bytes', $in->{response_bytes});
    if ($send_consumer_metric)  {
      push @metrics, gen_metric_int64(
              'serviceruntime.googleapis.com/api/consumer/response_bytes', $in->{response_bytes});
    }
  }

  if (exists $in->{error_type}) {
    push @metrics, gen_metric_int64(
      'serviceruntime.googleapis.com/api/producer/error_count', 1);
    if ($send_consumer_metric) {
      push @metrics, gen_metric_int64(
              'serviceruntime.googleapis.com/api/consumer/error_count', 1);
    }
  }

  $operation->{metricValueSets} = \@metrics;
  my $ret = {
    'operations' => [ $operation ]
  };
  $ret->{'serviceConfigId'} = $in->{serviceConfigId} if exists $in->{serviceConfigId};
  $ret->{'serviceName'} = $in->{serviceName} if exists $in->{serviceName};
  print Dumper $ret if $ENV{TEST_NGINX_VERBOSE};
  return $ret;
}

# Simulates a report aggregated by service_control client.
# It will aggregate N report bodys into 1 with all their metric
# values * N, and its LowEntries appended N times.
# The result can be used as expected report body to compare
# with the actual report body received by the service control server.
sub aggregate_report_body {
  my ($report_body, $count) = @_;

  foreach my $operation (@{$report_body->{operations}}) {
    foreach my $metric (@{$operation->{metricValueSets}}) {
      foreach my $value (@{$metric->{metricValues}}) {
        if (exists $value->{int64Value}) {
          $value->{int64Value} = $value->{int64Value} * $count;
        }
        if (exists $value->{distributionValue}) {
          my $dist = $value->{distributionValue};
          $dist->{count} = $dist->{count} * $count;
          my @new_buckets;
          for my $bucket (@{$dist->{bucketCounts}}) {
            push @new_buckets, $bucket * $count;
          }
          $dist->{bucketCounts} = \@new_buckets;
          $value->{distributionValue} = $dist;
        }
      }
    }
    my @new_logs;
    foreach my $i (1..$count) {
      push @new_logs, $operation->{logEntries}->[0];
    }
    $operation->{logEntries} = \@new_logs;
  }
}

sub strip_random_metrics{
  my ($json_obj, @random_metrics) = @_;

  my %random_metric_map = map { $_ => 1 } @random_metrics;

  if (exists $json_obj->{operations}) {
    my $operation = $json_obj->{operations}->[0];
    if (exists $operation->{metricValueSets}) {
      my @metric_value_sets;
      foreach my $metric (@{$operation->{metricValueSets}}) {
        if (not exists($random_metric_map{$metric->{metricName}})) {
          push @metric_value_sets, $metric;
        }
      }
      $operation->{metricValueSets} = \@metric_value_sets;
    }
  }
}

sub sort_metrics_by_name{
  my ($json_obj) = @_;
  if (exists $json_obj->{operations}) {
    my $operation = $json_obj->{operations}->[0];
    if (exists $operation->{metricValueSets}) {
      sub by_metric_name { $a->{metricName} cmp $b->{metricName} }
      my @metric_value_sets = sort by_metric_name @{$operation->{metricValueSets}};
      $operation->{metricValueSets} = \@metric_value_sets;
    }
  }
}

sub compare_json {
  my ($json, $expected) = @_;
  my $json_obj = ApiManager::decode_json($json);

  strip_random_metrics($json_obj, @http1_random_metrics);
  sort_metrics_by_name($json_obj);
  sort_metrics_by_name($expected);

  print Dumper $json_obj if $ENV{TEST_NGINX_VERBOSE};
  my %ignore_keys = map { $_ => "1" } qw(
    startTime endTime timestamp operationId request_latency_in_ms);
  return ApiManager::compare($json_obj, $expected, "", \%ignore_keys);
}

# HTTP2 header use a static huffman encoding. The mandatory date header
# makes response size random, thus we ignore the response_size here
sub compare_http2_report_json {
  my ($json, $expected) = @_;
  my $json_obj = ApiManager::decode_json($json);

  strip_random_metrics($json_obj, @http2_random_metrics);
  strip_random_metrics($expected, @http2_random_metrics);

  sort_metrics_by_name($json_obj);
  sort_metrics_by_name($expected);

  print Dumper $json_obj if $ENV{TEST_NGINX_VERBOSE};
  my %ignore_keys = map { $_ => "1" } qw(
    startTime endTime timestamp operationId request_latency_in_ms response_size_in_bytes);
  return ApiManager::compare($json_obj, $expected, "", \%ignore_keys);
}

sub service_agent {
  return "ESP/" . get_version();
}

sub get_version {
  my $version_file = "./src/nginx/version";
  open F, '<', $version_file or die "Can't open ${version_file}: $!";
  my $content = <F>;
  close F;
  chomp $content;
  return $content;
}

# A function to convert service control proto data format.
# 1) proto_data: the origin proto data need to be converted. It can be any of:
#      seerialized binary, text or json.
# 2) proto_type: protobuf message type: could be one of:
#      check_request, check_response, report_request, report_response
# 3) out_format: the output proto data format, could be one of
#      binary, text, or json
sub convert_proto {
  my ($proto_data, $proto_type, $out_format) = @_;
  my $cmd = './src/tools/service_control_json_gen';
  my ($sin, $sout);
  open2($sout, $sin, $cmd, "--stdin", "--$proto_type", "--$out_format") or die "Failed to open2";
  binmode $sin;
  print $sin $proto_data;
  close $sin;
  my $ret = join "", <$sout>;
  close $sout;
  return $ret
}

1;
