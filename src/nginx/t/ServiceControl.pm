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
# A shared module to generate ServiceControl report Perl object for testing.

package ServiceControl;

use ApiManager;
use strict;
use warnings;
use Status;
use IPC::Open2;

use Data::Dumper;

my @random_metrics = (
    'serviceruntime.googleapis.com/api/consumer/total_latencies',
    'serviceruntime.googleapis.com/api/producer/total_latencies',
    'serviceruntime.googleapis.com/api/consumer/backend_latencies',
    'serviceruntime.googleapis.com/api/producer/backend_latencies',
    'serviceruntime.googleapis.com/api/consumer/request_overhead_latencies',
    'serviceruntime.googleapis.com/api/producer/request_overhead_latencies',
    'serviceruntime.googleapis.com/api/producer/by_consumer/total_latencies',
    'serviceruntime.googleapis.com/api/producer/by_consumer/request_overhead_latencies',
    'serviceruntime.googleapis.com/api/producer/by_consumer/backend_latencies',
);

my %random_metric_map = map { $_ => 1 } @random_metrics;

sub gen_metric_int64 {
  my ($name, $value) = @_;
  return {
    'metricName' => $name,
    'metricValues' => [ { 'int64Value' => $value } ]
  };
}

my %time_distribution = (
  buckets => 8,
  growth => 10.0,
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
    'servicecontrol.googleapis.com/service_agent' => 'ESP',
    'servicecontrol.googleapis.com/user_agent' => 'ESP',
    'serviceruntime.googleapis.com/api_version' => $in->{api_name},
    'serviceruntime.googleapis.com/api_method' => $in->{api_method},
    'cloud.googleapis.com/location' => $in->{location},
    '/response_code' => $in->{response_code},
    '/status_code' => Status::http_response_code_to_status_code($in->{response_code}),
    '/response_code_class' => (($in->{response_code} / 100) % 10) . "xx",
    '/protocol' => 'http'
  };

  $labels->{'/status_code'} = $in->{status_code} if exists $in->{status_code};
  $labels->{'/error_type'} = $in->{error_type} if exists $in->{error_type};
  $labels->{'/protocol'} = $in->{protocol} if exists $in->{protocol};

  if (exists $in->{platform}) {
    $labels->{'servicecontrol.googleapis.com/platform'} = $in->{platform};
  } else {
    $labels->{'servicecontrol.googleapis.com/platform'} = 'unknown';
  }

  if (exists $in->{api_key}) {
    $labels->{'/credential_id'} = 'apiKey:' . $in->{api_key};
  } elsif (exists $in->{jwtAuth}) {
    $labels->{'/credential_id'} = 'jwtAuth:' . $in->{jwtAuth};
  }
  return $labels;
}

sub gen_log_entry {
  my $in = shift;

  my $payload = {
    'api_name' => $in->{api_name},
    'api_method' => $in->{api_method},
    'http_response_code' => $in->{response_code},
  };

  $payload->{producer_project_id} = $in->{producer_project_id} if
    exists $in->{producer_project_id};
  $payload->{api_key} = $in->{api_key} if exists $in->{api_key};
  $payload->{referer} = $in->{referer} if exists $in->{referer};
  $payload->{location} = $in->{location} if exists $in->{location};
  $payload->{request_size} = $in->{request_size} if exists $in->{request_size};
  $payload->{response_size} = $in->{response_size} if exists $in->{response_size};
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

  my $operation = {
    'operationName' => $in->{api_method},
  };
  if (exists $in->{api_key}) {
    $operation->{consumerId} = 'api_key:' . $in->{api_key};
  } elsif (exists $in->{producer_project_id}) {
    $operation->{consumerId} = 'project:' . $in->{producer_project_id};
  }

  my $ll = gen_report_labels($in);
  $operation->{labels} = $ll;

  my @log_entries = (gen_log_entry($in),);
  $operation->{logEntries} = \@log_entries;


  my @metrics = (
    gen_metric_int64(
      'serviceruntime.googleapis.com/api/consumer/request_count', 1),
    gen_metric_int64(
      'serviceruntime.googleapis.com/api/producer/request_count', 1),
    gen_metric_int64(
      'serviceruntime.googleapis.com/api/producer/by_consumer/request_count', 1),

    gen_metric_dist(\%size_distribution,
      'serviceruntime.googleapis.com/api/consumer/request_sizes', $in->{request_size}),
    gen_metric_dist(\%size_distribution,
      'serviceruntime.googleapis.com/api/producer/request_sizes', $in->{request_size}),
    gen_metric_dist(\%size_distribution,
      'serviceruntime.googleapis.com/api/producer/by_consumer/request_sizes', $in->{request_size}),
    );

  if (exists $in->{response_size}) {
    push @metrics, gen_metric_dist(\%size_distribution,
      'serviceruntime.googleapis.com/api/consumer/response_sizes', $in->{response_size});
    push @metrics, gen_metric_dist(\%size_distribution,
      'serviceruntime.googleapis.com/api/producer/response_sizes', $in->{response_size});
    push @metrics, gen_metric_dist(\%size_distribution,
      'serviceruntime.googleapis.com/api/producer/by_consumer/response_sizes', $in->{response_size});
  }

  if (exists $in->{error_type}) {
    push @metrics, gen_metric_int64(
      'serviceruntime.googleapis.com/api/consumer/error_count', 1);
    push @metrics, gen_metric_int64(
      'serviceruntime.googleapis.com/api/producer/error_count', 1);
    push @metrics, gen_metric_int64(
      'serviceruntime.googleapis.com/api/producer/by_consumer/error_count', 1);
  }

  $operation->{metricValueSets} = \@metrics;
  my $ret = {
    'serviceName' => $in->{api_name},
    'operations' => [ $operation ]
  };
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
  my ($json_obj) = @_;
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

  strip_random_metrics($json_obj);
  sort_metrics_by_name($json_obj);
  sort_metrics_by_name($expected);

  print Dumper $json_obj if $ENV{TEST_NGINX_VERBOSE};
  my %ignore_keys = map { $_ => "1" } qw(
    startTime endTime timestamp operationId request_latency_in_ms);
  return ApiManager::compare($json_obj, $expected, "", \%ignore_keys);
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
  my $cmd = $ENV{TEST_SRCDIR}."/src/tools/service_control_json_gen";
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
