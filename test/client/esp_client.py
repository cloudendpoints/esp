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
A ESP test client to drive HTTP load.
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

import esp_perfkit_publisher
import esp_wrk_runner
import esp_h2load_runner
import gflags as flags
import json
import sys
import time

from string import Template
from collections import Counter

FLAGS = flags.FLAGS

# Test suites are dict of name to list of a test cases,
#
# Each test cases contains five fields:
#   Runner: test execution module
#   n: number of requests
#   c: number of connections to ESP
#   t: number of threads
#   d: test duration in seconds
TEST_SUITES = {
        'debug': [
                (esp_wrk_runner, 0, 5, 2, 1)
                ],
        'simple': [
                (esp_wrk_runner, 0, 1, 1, 15),
                (esp_wrk_runner, 0, 5, 1, 15),
                (esp_wrk_runner, 0, 10, 1, 15),
                (esp_wrk_runner, 0, 15, 1, 15)
                ],
        'stress': [
                (esp_wrk_runner, 0, 50, 1, 60),
                (esp_wrk_runner, 0, 100, 1, 60),
                (esp_wrk_runner, 0, 100, 2, 60),
                (esp_wrk_runner, 0, 200, 1, 60),
                ],
        '2m_stress': [
                (esp_wrk_runner, 0, 1, 1, 120),
                (esp_wrk_runner, 0, 5, 1, 120),
                (esp_wrk_runner, 0, 10, 1, 120),
                (esp_wrk_runner, 0, 50, 1, 120),
                (esp_wrk_runner, 0, 50, 5, 120),
                (esp_wrk_runner, 0, 100, 1, 120),
                (esp_wrk_runner, 0, 100, 5, 120),
                ],
        'http2': [
                (esp_h2load_runner, 1000, 1, 1, 0)
                ]
        }

flags.DEFINE_enum(
        'test', 'simple', TEST_SUITES.keys(),
        'test suit name')

flags.DEFINE_string('test_env', '',
        'JSON test description')

flags.DEFINE_string('test_data', 'test_data.json.temp',
        'Template for test data')

flags.DEFINE_string('host', 'localhost:8080',
        'Server location')

flags.DEFINE_string('api_key', '',
        'API key')

flags.DEFINE_string('auth_token', '',
        'Authentication token')

flags.DEFINE_string('post_file', '',
        'File for request body content')

def count_failed_requests(out):
    """ Count failed and non-2xx responses """
    failed = 0
    non2xx = 0
    for metrics, _, _ in out:
        failed += metrics.get('Failed requests', [0])[0]
        non2xx += metrics.get('Non-2xx responses', [0])[0]
    return failed, non2xx

if __name__ == "__main__":
    try:
        argv = FLAGS(sys.argv)  # parse flags
    except flags.FlagsError as e:
        sys.exit('%s\nUsage: %s ARGS\n%s' % (e, sys.argv[0], FLAGS))

    test_env = {'test': FLAGS.test}
    if FLAGS.test_env:
        test_env.update(json.load(open(FLAGS.test_env, 'r')))

    if not FLAGS.test_data:
        sys.exit('Error: flag test_data is required')
    with open(FLAGS.test_data) as f:
        test_data = json.loads(Template(f.read()).substitute(
                HOST=FLAGS.host,
                API_KEY=FLAGS.api_key,
                JWT_TOKEN=FLAGS.auth_token,
                POST_FILE=FLAGS.post_file))

        print "=== Test data"
    print json.dumps(test_data)

    results = []
    for run in test_data['test_run']:
        for runner, n, c, t, d in TEST_SUITES[FLAGS.test]:
            ret = runner.test(run, n, c, t, d)
            if ret:
                metrics, errors = ret

                print '=== Metric:'
                for k in metrics.keys():
                    print k, metrics[k][0], metrics[k][1]

                print '=== Metadata:'
                metadata = {
                        'runner': runner.__name__,
                        'number': str(n),
                        'concurrent': str(c),
                        'threads': str(t),
                        'duration': str(d) + 's',
                        'time': time.time(),
                        }
                if 'labels' in run:
                    metadata.update(run['labels'])
                print json.dumps(metadata)

                if len(errors) > 0:
                    print '=== Error status responses:'
                    for error, count in Counter(errors).most_common():
                        print '= %06d: %s' % (count, error)

                results.append((metrics, metadata, errors))

    if not results:
        sys.exit('All load tests failed.')
    if FLAGS.test_env:
        esp_perfkit_publisher.Publish(results, test_env)

    failed, non2xx = count_failed_requests(results)
    if failed + non2xx > 0:
        sys.exit(
            ('Load test failed:\n'
             '  {} failed requests,\n'
             '  {} non-2xx responses.').format(failed, non2xx))

    print "All load tests are successful."
