#!/usr/bin/python -u
#
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

"""Tests for ESP wrapped bookstore application deployed in VM.

Testing steps:
1) Deploy bookstore application in a VM instance.

   HOST=deployed_host_name

   To use https protocol, HOST should started with https://
   Otherwise http protocol will be used.

2) Use following step to generate auth token

   cd ${ESP}/client/custom
   SERVICE_NAME=${PROJECT_ID}.appspot.com
   TOKEN=`./gen-auth-token.sh -a ${SERVICE_NAME}`

3) Get api_key from Pantheon UI of your project.

   KEY=api_key of your project

4) Run:

   ./esp_bookstore_quota_test.py --host=$HOST --api_key=$KEY --auth_token=$TOKEN
"""

import argparse
import esp_utils
import httplib
import json
import ssl
import sys
import os
import time
import datetime
from esp_utils import EspClientTest

class C:
    pass
FLAGS = C

class EspBookstoreTest(EspClientTest):
    """End to end integration test of bookstore application with deployed
    ESP at VM.  It will call bookstore API according its Swagger spec
    1) set quota limit to 30
    2) send traffic 60 qps for 150 seconds and count response code 200
    3) check count between 80 to 100
    """

    def __init__(self):
        EspClientTest.__init__(self, FLAGS.host,
                               FLAGS.allow_unverified_cert,
                               FLAGS.verbose)

    def verify_quota_control(self):
        # turn off verbose log
        print("Turn off the verbose log flag...");
        verbose = FLAGS.verbose
        FLAGS.verbose = False
        self.set_verbose(FLAGS.verbose)

        # exhaust current quota
        print("Exhaust current quota...");
        response = self._call_http(path='/quota_read',
                                   api_key=FLAGS.api_key)
        if response.status_code != 429:
            while True:
                response = self._call_http(path='/quota_read',
                                           api_key=FLAGS.api_key)
                if response.status_code == 429:
                    break;

        # waiting for the next quota refill.
        print("Wait for the next quota refill...");
        while True:
            time.sleep(1);
            response = self._call_http(path='/quota_read',
                                       api_key=FLAGS.api_key)
            if response.status_code != 429:
                break;

        # start counting
        print("Sending requests to count response codes for 150 seconds...");
        code_200 = 0
        code_429 = 0
        code_else = 0

        # run tests for 150 seconds, two quota refills expected
        t_end = time.time() + 60 * 2 + 30
        count = 0;
        while time.time() < t_end:
            response = self._call_http(path='/quota_read',
                                       api_key=FLAGS.api_key)
            if response.status_code == 429:
                code_429 += 1
            elif response.status_code == 200:
                code_200 += 1
            else:
                code_else += 1
            count += 1

            print(t_end - time.time(), code_200, code_429, code_else)
            # delay 1 second after each request
            time.sleep(1);

        self.assertEqual(code_else, 0);

        # requests should be accepted between 81 to 99
        # by initial quota and 2 refills. Allows +-10% of margin
        self.assertGE(code_200 , 81);
        self.assertLE(code_200 , 99);

        # restore verbose flag
        FLAGS.verbose = verbose
        self.set_verbose(FLAGS.verbose)

    def run_all_tests(self):

        self.verify_quota_control()

        if self._failed_tests:
            sys.exit(esp_utils.red('%d tests passed, %d tests failed.' % (
                self._passed_tests, self._failed_tests)))
        else:
            print esp_utils.green('All %d tests passed' % self._passed_tests)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--verbose', type=bool, help='Turn on/off verbosity.')
    parser.add_argument('--api_key', help='Project api_key to access service.')
    parser.add_argument('--host', help='Deployed application host name.')
    parser.add_argument('--auth_token', help='Auth token.')
    parser.add_argument('--allow_unverified_cert', type=bool,
            default=False, help='used for testing self-signed ssl cert.')
    flags = parser.parse_args(namespace=FLAGS)

    esp_test = EspBookstoreTest()
    try:
        esp_test.run_all_tests()
    except KeyError as e:
        sys.exit(esp_utils.red('Test failed.'))
