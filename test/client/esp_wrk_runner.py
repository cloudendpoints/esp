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
import json
import os
import pprint
import time

from string import Template
from collections import Counter

WRK_PATH = os.environ.get('WRK_PATH', '/usr/local/bin/wrk')

def test(run, c, t, d):
    """Run a test and extract its results.
    run: is a dict {
       'url': a string
       'headers': [headers]
       'post_file': a string
       }
    c: number of connections that wrk makes to ESP
    t: number of threads
    d: the timelimit of the test, in seconds
    return a tuple of (metric, metadata)
    metric: is a dict of metric name to a tuple of (value, unit)
    metadata: is per test metadata such time, n and c.
    """
    cmd = [WRK_PATH,
            '-t', str(t),
            '--timeout', '2m',
            '-c', str(c),
            '-d', str(d) + 's',
            '-s', 'wrk_script.lua',
            '-H', '"Content-Type:application/json"']

    if 'headers' in run:
        for h in run['headers']:
            cmd += ['-H', h]

    if 'post_file' in run:
        wrk_method = "POST"
        wrk_body_file = run['post_file']
    else:
        wrk_method = "GET"
        wrk_body_file = "/dev/null"

    wrk_out = 'wrk_out'
    wrk_err = 'wrk_err'
    with open('wrk_script.lua.temp', 'r') as f:
        wrk_script = f.read()

    with open('wrk_script.lua', 'w') as f:
        f.write(Template(wrk_script).substitute(
                HTTP_METHOD=wrk_method,
                REQUEST_BODY_FILE=wrk_body_file,
                OUT=wrk_out,
                ERR=wrk_err))

        cmd += [run['url']]
    (_, ret) = esp_utils.IssueCommand(cmd)

    if ret != 0:
        print '==== Failed to run=%s,t=%d,c=%s,ret=%d' % (str(run), t, c, ret)
        return None

    with open(wrk_out, 'r') as f:
        metrics = json.load(f)

    # Print out these fields for easy awk extraction in release-qualify script
    for k, nice_key in [('Requests', 'Complete requests: '),
                        ('Failed requests', 'Failed requests: '),
                        ('Non-2xx responses', 'Non-2xx responses: ')]:
      if metrics.get(k, None):
        print "%s %s" % (nice_key, metrics[k][0])

    for k in metrics.keys():
        metrics[k] = tuple(metrics[k])
    print '==== Metrics:'
    pprint.pprint(metrics)
    metadata = {
            'concurrent': str(c),
            'threads': str(t),
            'duration': str(d) + 's',
            'time': time.time(),
            }
    print '=== Metadata:'
    pprint.pprint(metadata)
    if 'labels' in run:
        metadata.update(run['labels'])

    errors = []
    for i in range(0, t):
        with open(wrk_err + '_' + str(i), 'r') as f:
            errors.extend(f.readlines())

    if len(errors) > 0:
        print '=== Error status responses:'
        for error, count in Counter(errors).most_common():
            print '= %06d: %s' % (count, error)

    return metrics, metadata, errors
