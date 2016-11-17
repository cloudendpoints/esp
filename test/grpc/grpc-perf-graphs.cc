// Copyright (C) Extensible Service Proxy Authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//
////////////////////////////////////////////////////////////////////////////////
//
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include "google/protobuf/text_format.h"
#include "test/grpc/client-test-lib.h"
#include "test/grpc/grpc-test.grpc.pb.h"

using ::google::protobuf::TextFormat;

namespace test {
namespace grpc {
namespace {

const char kHelloWorld[] = "Hello, world!";

void AddEchoRun(TestPlans* plans, std::int32_t parallel_limit,
                std::int32_t test_count) {
  auto parallel = plans->add_plans()->mutable_parallel();
  parallel->set_parallel_limit(parallel_limit);
  parallel->set_test_count(test_count);
  auto subtest = parallel->add_subtests();
  subtest->set_weight(1);
  auto echo = subtest->mutable_echo();
  echo->mutable_request()->set_text(kHelloWorld);
}

void RunMeasurements(const char* server_address) {
  TestPlans plans;
  TestResults results;

  const int kMicroPerSec = 1000000;

  // N.B. Each plan is matched by one result.
  // When altering a plan (or a loop that generates a plan), make sure
  // to edit the corresponding point where the result is consumed.

  plans.set_server_addr(server_address);

  // Warmup.
  AddEchoRun(&plans, 10, 100);

  // The total number of test calls to issue per run.  This should be
  // larger than kParallelLimit, in order to actually drive a
  // sufficient number of calls through for the parallel limit to
  // matter.
  const int kTestCount = 2000;

  // The limit on the number of calls to issue in parallel.
  const int kParallelLimit = 1000;

  // The number of runs to make for each measurement.  The more runs,
  // the tighter the standard deviation in the results should become,
  // but the longer it will take to make each measurement.
  const int kRunCount = 5;

  // The initial number of parallel calls to try, and the increment
  // for the number of parallel calls, up to kParallelLimit.
  const int kParallelIncrement = 10;

  for (int parallel_limit = kParallelIncrement;
       parallel_limit <= kParallelLimit; parallel_limit += kParallelIncrement) {
    for (int run = 0; run < kRunCount; run++) {
      AddEchoRun(&plans, parallel_limit, kTestCount);
    }
  }

  ::test::grpc::RunTestPlans(plans, &results);

  int result_index = 0;

  // Consume warmup.
  result_index++;

  // Consume kTestCount calls at various parallel limits:
  std::cout << "# parallel_limit qps qps_low qps_high "
            << "latency_ms latency_ms_stddev" << std::endl;
  for (int parallel_limit = kParallelIncrement;
       parallel_limit <= kParallelLimit; parallel_limit += kParallelIncrement) {
    // Calculate the time_mean and time_stddev, in micros
    double total = 0;
    for (int run = 0; run < kRunCount; run++) {
      auto& parallel = results.results(result_index + run).parallel();
      total += parallel.total_time_micros();
    }
    double time_mean = total / kRunCount;
    double qps_mean = ((double)kTestCount) / (time_mean / kMicroPerSec);

    double sum_diffs = 0;
    for (int run = 0; run < kRunCount; run++) {
      auto& parallel = results.results(result_index + run).parallel();
      double qps = ((double)kTestCount) /
                   (((double)parallel.total_time_micros()) / kMicroPerSec);
      double delta = qps_mean - qps;
      sum_diffs += delta * delta;
    }
    double variance = sum_diffs / kRunCount;
    double qps_stddev = std::sqrt(variance);

    // Calculate the latency_mean and latency_stddev, in micros
    total = 0;
    variance = 0;
    for (int run = 0; run < kRunCount; run++) {
      auto& parallel = results.results(result_index + run).parallel();
      auto& stats = parallel.stats(0);
      total += stats.mean_latency_micros();
      variance +=
          (stats.stddev_latency_micros() * stats.stddev_latency_micros());
    }
    double latency_mean = total / kRunCount;
    double latency_stddev = std::sqrt(variance / kRunCount);

    // And print them out, converting latency to millis.
    std::cout << parallel_limit << " " << qps_mean << " " << qps_stddev << " "
              << (latency_mean / 1000) << " " << (latency_stddev / 1000)
              << std::endl;

    result_index += kRunCount;
  }

  // Verify that all results are consumed.
  if (result_index != results.results_size()) {
    std::cerr << "Internal error: result count mismatch (expected "
              << result_index << ", got " << results.results_size() << ")"
              << std::endl;
    std::abort();
  }
}

}  // namespace
}  // namespace grpc
}  // namespace test

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: grpc-perf-graphs <server address>" << std::endl;
    return EXIT_FAILURE;
  }

  ::test::grpc::RunMeasurements(argv[1]);

  return EXIT_SUCCESS;
}
