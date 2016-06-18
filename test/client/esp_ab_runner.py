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

import esp_utils
import os
import pprint
import re
import sys
import time


AB_PATH = os.environ.get('AB_PATH', '/usr/local/apache2/bin/ab')

RE_MEAN_LATENCY = re.compile(r'Time per request:\s+(\d+\.\d+) \[ms\] \(mean\)')
RE_MEAN_LATENCY_ACROSS = re.compile(
    r'Time per request:\s+(\d+\.\d+) \[ms\] \(mean, across all')
RE_PERCENTILE_START_LINE = re.compile(
    r'Percentage of the requests served within a certain time \(ms\)')
RE_PERCENTILE = re.compile(r'\s+(\d+%)\s+(\d+)')

METRIC_MEAN_LATENCY = 'mean'
METRIC_MEAN_LATENCY_ACROSS = 'mean across threads'
METRIC_PERCENTILE_PREFIX = 'Latency percentile: '

# A list of (RE, metric_name, unit)
MATCH_LIST = [
    # Non-2xx responses
    (re.compile(r'Non-2xx responses:\s+(\d+)'), 'Non-2xx responses', 'number'),
    # Failed request
    (re.compile(r'Failed requests:\s+(\d+)'), 'Failed requests', 'number'),
    # Transfer rate
    (re.compile(
        r'Transfer rate:\s+(\d+\.\d+) \[Kbytes/sec\] received'), 'Transfer rate', 'Kbytes/sec'),
    # Request per second
    (re.compile(
        r'Requests per second:\s+(\d+\.\d+) \[#/sec\]'), 'Requests per second', 'qps'),
]

def ExtractMetrics(out):
    """Extract metrics from the output of ab.
    out: a string from ab stdout.
    return: a dict of
       metric name => (metric value, metric unit)
    """
    # If all requests are OK, AB will not show Non-2xx line.
    # For consistency, initialize this metric as 0.
    metrics = {'Non-2xx responses': (0.0, 'number')}
    at_percentile = False
    for line in out.splitlines():
        # Last part of percentile portion.
        if RE_PERCENTILE_START_LINE.match(line):
            at_percentile = True
            continue
        if at_percentile:
            m = RE_PERCENTILE.match(line)
            if m:
                metrics[METRIC_PERCENTILE_PREFIX + m.group(1)] = (
                    float(m.group(2)), 'ms')
            continue

        m = RE_MEAN_LATENCY.match(line)
        if m:
            metrics[METRIC_PERCENTILE_PREFIX + METRIC_MEAN_LATENCY] = (
                float(m.group(1)), 'ms')
            continue
        m = RE_MEAN_LATENCY_ACROSS.match(line)
        if m:
            metrics[METRIC_PERCENTILE_PREFIX + METRIC_MEAN_LATENCY_ACROSS] = (
                float(m.group(1)), 'ms')
            continue
        for reg, metric, unit in MATCH_LIST:
            m = reg.match(line)
            if m:
                metrics[metric] = (float(m.group(1)), unit)
                break
    return metrics

def RunTest(run, n, c, t, d):
    """Run a ab test and extract its results.
    run: is a dict {
       'url': a string
       'headers': [headers]
       'post_file': a string
       }
    return a tuple of (metric, metadata)
    metric: is a dict of metric name to a tuple of (value, unit)
    metadata: is per test metadata such time, n and c.
    """
    # -t implies "-n 50000", so -n parameter must be after -t
    cmd = [AB_PATH, '-t', str(d), '-n', str(n), '-c', str(c), '-q']
    # bookstore/echo requires content-type as application/json
    cmd += ['-T', 'application/json']
    # -l avoids reporting errors if the length of the responses is not constant.
    cmd += ['-l']
    if 'headers' in run:
        for h in run['headers']:
            cmd += ['-H', h]
    if 'post_file' in run:
        cmd += ['-p', run['post_file']]
    cmd += [run['url']]
    (out, ret) = esp_utils.IssueCommand(cmd)
    if ret != 0:
        print '==== Failed to run=%s, n=%d, c=%d' % (str(run), n, c)
        return None
    metrics = ExtractMetrics(out)
    print '==== Metrics:'
    pprint.pprint(metrics)
    metadata = {
        'requests': str(n),
        'concurrent': str(c),
        'time': time.time(),
    }
    if 'labels' in run:
        metadata.update(run['labels'])
    return metrics, metadata
