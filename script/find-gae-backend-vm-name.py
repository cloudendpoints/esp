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

import sys
import json

"""
This Python script processes JSON output from the gcloud command:

gcloud app instances list ...

The command outputs list of virtual machine instances managed by
Google App Engine. This script extracts:
 * virtual machine zone
 * virtual machine id (4-character random string used by GAE)
 * virtual machine name

An abbreviated example of input processed by this script is (note
the JSON array wrapping the list of VM instances):

[
  {
    "id": "y8fl",
    "instance": {
      "name": "gae-default-rc--test--20160325150701--8-y8fl",
      "zone": "us-central1-c",

      ...

    },
    "version": "rc-test-20160325150701-8"
  }
]

"""

def main():
  try:
    instances = json.load(sys.stdin)
  except:
    sys.stderr.write("Cannot parse JSON\n")
    return

  for elem in instances:
    # 'id' is the 4-character random string used by GAE to identify the VM
    id = elem.get("id", None)
    if id is None:
      continue

    # 'instance' is a nested JSON object which contains more details about
    # the compute engine instance (it is likely the compute engine instance
    # resources, inlined verbatim)
    i = elem.get("instance", None)
    if i is None:
      continue

    # we need to extract zone and name so we can later SSH to the machine
    zone = i.get("vmZoneName", "")
    name = i.get("vmName", "")

    # try older version format
    if not zone or not name:
      zone = i.get("zone", "")
      name = i.get("name", "")

    if not zone or not name:
      continue

    sys.stdout.write("{} {} {}\n".format(id, zone, name))

if __name__ == "__main__":
  main()
