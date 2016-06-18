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

import subprocess

def IssueCommand(cmd, force_info_log=False, suppress_warning=False,
                 env=None):
    """Tries running the provided command once.
    Args:
      cmd: A list of strings such as is given to the subprocess.Popen()
          constructor.
      force_info_log: A boolean indicating whether the command result should
          always be logged at the info level. Command results will always be
          logged at the debug level if they aren't logged at another level.
      suppress_warning: A boolean indicating whether the results should
          not be logged at the info level in the event of a non-zero
          return code. When force_info_log is True, the output is logged
          regardless of suppress_warning's value.
      env: A dict of key/value strings, such as is given to the subprocess.Popen()
          constructor, that contains environment variables to be injected.
    Returns:
      A tuple of stdout, and retcode from running the provided command.
    """
    print '==== Running: %s' % str(cmd)
    process = subprocess.Popen(cmd, env=env,
                               stdout=subprocess.PIPE)
    stdout = ''
    while True:
        output = process.stdout.readline()
        if output == '' and process.poll() is not None:
            break
        if output:
            stdout += output
            print output.strip()
    rc = process.poll()
    return stdout, rc
