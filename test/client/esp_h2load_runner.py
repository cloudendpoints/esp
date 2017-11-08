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

import esp_utils
import json
import os
import pprint
import time
import re

from string import Template
from collections import Counter

H2LOAD = os.environ.get('H2LOAD', '/nghttp2/src/h2load')

def test(run, n, c, t, d):
    """Run a test and extract its results.

    Args:
        run: is a dict {
           'url': a string
           'headers': [headers]
           'post_file': a string
           }
        n: number of requests
        c: number of connections
        t: number of threads
        d: test duration in seconds

    Returns:
        metric: is a dict of metric name to a tuple of (value, unit)
        metadata: is per test metadata such time, n and c.
        errors: a list of non-200 responses
    """
    cmd = [H2LOAD,
            '-n', str(n),
            '-t', str(t),
            '-c', str(c),
            '-r', str(1),
            '-H', '"Content-Type:application/json"']

    if 'headers' in run:
        for h in run['headers']:
            cmd += ['-H', h]

    if 'post_file' in run:
        cmd += ['-d', run['post_file']]

    cmd += [run['url']]

    (out, ret) = esp_utils.IssueCommand(cmd)

    if ret != 0:
        print '==== Failed to run'
        return None

    metrics = {}

    # h2load does not output non-2xx error responses
    errors = []

    # Parse the output of h2load
    for line in out.split("\n"):
        print line
        if line.startswith('requests:'):
            r = re.search(r'requests: (\d+) total, (\d+) started, (\d+) done, (\d+) succeeded, (\d+) failed, (\d+) errored, (\d+) timeout', line)
            metrics['Complete requests'] = (int(r.group(4)), 'number')
            metrics['Failed requests'] = (int(r.group(5)), 'number')
            metrics['Timeout requests'] = (int(r.group(7)), 'number')
        if line.startswith('finished in'):
            r = re.search(r'finished in (\d+\.?\d+\w+), (\d+\.?\d+) req/s', line)
            metrics['Requests per second'] = (r.group(2), 'qps')
        if line.startswith('status codes:'):
            r = re.search(r'status codes: (\d+) 2xx, (\d+) 3xx, (\d+) 4xx, (\d+) 5xx', line)
            metrics['Non-2xx responses'] = (int(r.group(2)) + int(r.group(3)) + int(r.group(4)), 'number')
        if line.startswith('time for request:'):
            r = re.search('time for request:\s+(\d+\.?\d+)(\w+)\s+(\d+\.?\d+)(\w+)\s+(\d+\.?\d+)(\w+)\s+(\d+\.?\d+)(\w+)\s+(\d+\.?\d+)%', line)
            metrics['Latency percentile: 100%'] = (r.group(3), r.group(4))
            metrics['Latency percentile: mean'] = (r.group(5), r.group(6))

    return metrics, errors

