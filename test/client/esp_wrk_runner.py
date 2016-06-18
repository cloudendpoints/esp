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
import re
import sys
import time


WRK_PATH = os.environ.get('WRK_PATH', '/usr/local/bin/wrk')

WRK_SCRIPT = '''
function read(file)
    local f = io.open(file, "rb")
    local content = f:read("*all")
    f:close()
    return content
end

done = function(summary, latency, requests)
   io.write("JSON:")
   io.write("{")
   errors = summary.errors
   failed = errors.connect + errors.read + errors.write + errors.timeout
   io.write(string.format("\\"Failed requests\\":[%d,\\"number\\"],", failed));
   io.write(string.format("\\"Failed requests\\":[%d,\\"number\\"],", failed));
   for _, p in pairs({50, 66, 75, 80, 90, 95, 98, 99}) do
      n = latency:percentile(p)
      io.write(string.format("\\"Latency percentile: %d%%\\":[%g,\\"ms\\"],", p, n / 1000.0))
   end
   io.write(string.format("\\"Latency percentile: mean\\":[%g,\\"ms\\"],", latency.mean / 1000.0))
   io.write(string.format("\\"Non-2xx responses\\":[%d,\\"number\\"],", errors.status))
   io.write(string.format("\\"Requests per second\\":[%g,\\"qps\\"],", summary.requests / summary.duration * 1000000))
   io.write(string.format("\\"Transfer rate\\":[%g,\\"Kbytes/sec\\"]", summary.bytes / 1024 / summary.duration * 1000000))

   io.write("}\\n")
end

wrk.method = "__METHOD__"
wrk.body   = __BODY__
wrk.headers["Content-Type"] = "application/json"
'''

def ExtractMetrics(out):
    """Extract metrics from the output of wrk.
    out: a string from wrk stdout with report.lua script.
    return: a dict of
       metric name => (metric value, metric unit)
    """
    metrics_json = out.split("JSON:")[1]
    metrics = json.loads(metrics_json)
    for k in metrics.keys():
        metrics[k] = tuple(metrics[k])

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
    cmd = [WRK_PATH, '-t', str(t), '-c', str(c),
           '-d', str(d) + 's', '-s', 'wrk_script.lua']
    if 'headers' in run:
        for h in run['headers']:
            cmd += ['-H', h]
    method = "GET"
    body = "\"\""
    if 'post_file' in run:
        method = "POST"
        body = "read(\"%s\")" % run['post_file']
    with open("wrk_script.lua", "w") as f:
        f.write(WRK_SCRIPT.replace("__METHOD__", method).replace("__BODY__", body))
    cmd += [run['url']]
    (out, ret) = esp_utils.IssueCommand(cmd)
    if ret != 0:
        print '==== Failed to run=%s, n=%s, c=%s' % (str(run), n, c)
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
