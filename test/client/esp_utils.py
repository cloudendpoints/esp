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

import httplib
import json
import ssl
import subprocess

def IssueCommand(cmd, force_info_log=False, suppress_warning=False,
        env=None):
    """Tries running the provided command once.
    Args:
      cmd: A list of strings such as is given to the subprocess.Popen()
          constructor.
      env: A dict of key/value strings, such as is given to the subprocess.Popen()
          constructor, that contains environment variables to be injected.
    Returns:
      A tuple of stdout, and retcode from running the provided command.
    """
    print '=== Running: %s' % ' '.join(cmd)
    process = subprocess.Popen(cmd, env=env,
            stdout=subprocess.PIPE)
    stdout = ''
    while True:
        output = process.stdout.readline()
        if output == '' and process.poll() is not None:
            break
        if output:
            stdout += output
            print '= ' + output.strip()
    rc = process.poll()
    print '=== Finished with code %d' % rc
    return stdout, rc

COLOR_RED = '\033[91m'
COLOR_GREEN = '\033[92m'
COLOR_END = '\033[0m'

HTTPS_PREFIX = 'https://'
HTTP_PREFIX = 'http://'

def green(text):
    return COLOR_GREEN + text + COLOR_END

def red(text):
    return COLOR_RED + text + COLOR_END

def http_connection(host, allow_unverified_cert):
  if host.startswith(HTTPS_PREFIX):
      host = host[len(HTTPS_PREFIX):]
      print 'Use https to connect: %s' % host
      if allow_unverified_cert:
          try:
            return httplib.HTTPSConnection(
                host, timeout=5, context=ssl._create_unverified_context())
          except AttributeError:
            # Legacy versions of python do not check certificate.
            return httplib.HTTPSConnection(
                host, timeout=5)
      else:
          return httplib.HTTPSConnection(host)
  else:
      if host.startswith(HTTP_PREFIX):
          host = host[len(HTTP_PREFIX):]
      else:
          host = host
      print 'Use http to connect: %s' % host
      return httplib.HTTPConnection(host)

class Response(object):
    """A class to wrap around httplib.response class."""

    def __init__(self, r):
        self.text = r.read()
        self.status_code = r.status
        self.headers = r.getheaders()
        self.content_type = r.getheader('content-type')
        if self.content_type != None:
            self.content_type = self.content_type.lower()

    def json(self):
        try:
            return json.loads(self.text)
        except ValueError as e:
            print 'Error: failed in JSON decode: %s' % self.text
            return {}

    def is_json(self):
        if self.content_type != 'application/json':
            return False
        try:
            json.loads(self.text)
            return True
        except ValueError as e:
            return False

