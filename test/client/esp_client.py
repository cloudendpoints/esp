#!/usr/bin/python -u
#
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

"""
A ESP test client to use AB (Apache Benchmark) to drive HTTP load.
Its results will be uploaded to a BigQuery table in esp-test-dashboard
project so they can be showed by its dashboard.

1) Deploy ESP in a GCE VM, get its HTTP URL.
2) Start a test VM with this script.
3) If test VM is started from a different project than esp-test-dashboard,
add its service account to esp-test-dashboard project with role 'can edit'.
4) Run this script as:  esp_client.py --url=xxx

Note: Must run the script in the same folder where file
results_table_schema.json is.
"""

import esp_ab_runner
import esp_perfkit_publisher
import esp_wrk_runner
import gflags as flags
import json
import sys


FLAGS = flags.FLAGS

INF = 100000000

# Test suites are dict of name to list of a test cases,
# Each test cases contains five fields:
#  Runner: esp_ab_runner or esp_wrk_runner
#  Number of requests: only effective for ab
#  Connection/Concurrency: number of connection that ab/wrk makes to ESP
#  Thread: number of threads, only effective for wrk
#  Duration: the timelimit of the test, in seconds
TEST_SUITES = {
    'debug': [(esp_ab_runner, 100, 5, 1, INF)],
    'simple': [
        (esp_ab_runner, 10000, 1, None, INF),
        (esp_ab_runner, 10000, 5, None, INF),
        (esp_ab_runner, 10000, 10, None, INF),
        (esp_ab_runner, 10000, 15, None, INF)
    ],
    'stress': [
        (esp_ab_runner, 20000, 50, None, INF),
        (esp_ab_runner, 20000, 100, None, INF),
        (esp_ab_runner, 20000, 150, None, INF),
        (esp_ab_runner, 20000, 200, None, INF)
    ],
    'wrk_debug': [(esp_wrk_runner, 0, 1, 5, 1)],
    '1s_debug': [
        (esp_ab_runner, INF, 5, None, 1),
        (esp_wrk_runner, None, 5, 1, 1)
    ],
    '2m_stress': [
        (esp_ab_runner, INF, 1, None, 120),
        (esp_ab_runner, INF, 5, None, 120),
        (esp_ab_runner, INF, 10, None, 120),
        (esp_ab_runner, INF, 50, None, 120),
        (esp_ab_runner, INF, 100, None, 120),
        (esp_wrk_runner, None, 1, 1, 120),
        (esp_wrk_runner, None, 5, 1, 120),
        (esp_wrk_runner, None, 10, 1, 120),
        (esp_wrk_runner, None, 50, 1, 120),
        (esp_wrk_runner, None, 50, 5, 120),
        (esp_wrk_runner, None, 100, 1, 120),
        (esp_wrk_runner, None, 100, 5, 120),
    ]
}

flags.DEFINE_enum(
    'test', 'simple', TEST_SUITES.keys(),
    'test suit name')

flags.DEFINE_string('test_env', '',
                    ('A json file path to describe the test, some of '
                     'fields will be set to the metric labels.'))

flags.DEFINE_string('test_data', '',
                    'A json file path with test data')


def ReadJsonFile(file):
    return json.load(open(file, 'r'))

def CountFailedRequests(results):
    failed = 0
    for metrics, meta in results:
        failed += metrics.get('Failed requests', [0])[0]
        failed += metrics.get('Non-2xx responses', [0])[0]
    return failed

if __name__ == "__main__":
    try:
        argv = FLAGS(sys.argv)  # parse flags
    except flags.FlagsError as e:
        sys.exit('%s\nUsage: %s ARGS\n%s' % (e, sys.argv[0], FLAGS))

    test_env = {'test': FLAGS.test}
    if FLAGS.test_env:
        test_env.update(ReadJsonFile(FLAGS.test_env))

    if not FLAGS.test_data:
        sys.exit('Error: flag test_data is required')
    test_data = ReadJsonFile(FLAGS.test_data)

    results = []
    for run in test_data['test_run']:
        for runner, n, c, t, d in TEST_SUITES[FLAGS.test]:
            ret = runner.RunTest(run, n, c, t, d)
            if ret:
                results.append(ret)

    if not results:
        sys.exit('All load tests failed.')
    if FLAGS.test_env:
        esp_perfkit_publisher.Publish(results, test_env)
    if CountFailedRequests(results) > 0:
        sys.exit('Some load tests failed.')
    print "All load tests are successful."
