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

from string import Template

WRK_PATH = os.environ.get('WRK_PATH', '/usr/local/bin/wrk')

def test(run, n, c, t, d):
    """Run a test and extract its results.

    Args:
        run: is a dict {
           'url': a string
           'headers': [headers]
           'post_file': a string
           }
        n: number of requests (ignored by wrk)
        c: number of connections
        t: number of threads
        d: test duration in seconds

    Returns:
        metrics: is a dict of metric name to a tuple of (value, unit)
        errors: a list of non-200 responses
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

    expected_status = run.get('expected_status', '200')
    with open('wrk_script.lua', 'w') as f:
        f.write(Template(wrk_script).substitute(
                HTTP_METHOD=wrk_method,
                REQUEST_BODY_FILE=wrk_body_file,
                EXPECTED_STATUS=expected_status,
                OUT=wrk_out,
                ERR=wrk_err))

    cmd += [run['url']]

    (_, ret) = esp_utils.IssueCommand(cmd)

    if ret != 0:
        print '==== Failed to run=%s,t=%d,c=%s,ret=%d' % (str(run), t, c, ret)
        return None

    with open(wrk_out, 'r') as f:
        metrics = json.load(f)

    for k in metrics.keys():
        metrics[k] = tuple(metrics[k])

    errors = []
    for i in range(0, t):
        with open(wrk_err + '_' + str(i), 'r') as f:
            errors.extend(f.readlines())

    return metrics, errors
