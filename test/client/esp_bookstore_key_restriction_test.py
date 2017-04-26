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

   ./esp_bookstore_test.py --host=$HOST --api_key=$KEY --auth_token=$TOKEN
"""

import argparse
import esp_utils
import httplib
import json
import ssl
import sys
import os
import time

class C:
    pass
FLAGS = C

class EspBookstoreTest(object):
    """End to end integration test of bookstore application with deployed
    ESP at VM.  It will call bookstore API according its Swagger spec to
    1) wipe out bookstore clean,
    2) create 2 shelves,
    3) add 2 books into the first shelf.
    4) remove books one by one,
    5) remove shelves one by one.
    """

    def __init__(self):
        self._failed_tests = 0
        self._passed_tests = 0
        self.conn = esp_utils.http_connection(FLAGS.host, FLAGS.allow_unverified_cert)

    def fail(self, msg):
        print '%s: %s' % (esp_utils.red('FAILED'), msg if msg else '')
        self._failed_tests += 1

    def assertEqual(self, a, b):
        msg = 'assertEqual(%s, %s)' % (str(a), str(b))
        if a == b:
            print '%s: %s' % (esp_utils.green('OK'), msg)
            self._passed_tests += 1
        else:
            self.fail(msg)

    def assertGE(self, a, b):
        msg = 'assertGE(%s, %s)' % (str(a), str(b))
        if a >= b:
            print '%s: %s' % (esp_utils.green('OK'), msg)
            self._passed_tests += 1
        else:
            self.fail(msg)

    def assertLE(self, a, b):
        msg = 'assertLE(%s, %s)' % (str(a), str(b))
        if a <= b:
            print '%s: %s' % (esp_utils.green('OK'), msg)
            self._passed_tests += 1
        else:
            self.fail(msg)

    def _call_http(self, path, api_key=None, auth=None, data=None, method=None,
                   userHeaders = {}):
        """Makes a http call and returns its response."""
        url = path
        if api_key:
            url += '?key=' + api_key
        headers = {'Content-Type': 'application/json'}
        if auth:
            headers['Authorization'] = 'Bearer ' + auth
        body = json.dumps(data) if data else None
        for key, value in userHeaders.iteritems():
            headers[key] = value
        if not method:
            method = 'POST' if data else 'GET'
        if FLAGS.verbose:
            print 'HTTP: %s %s' % (method, url)
            print 'headers: %s' % str(headers)
            print 'body: %s' % body
        self.conn.request(method, url, body, headers)
        response = esp_utils.Response(self.conn.getresponse())
        if FLAGS.verbose:
            print 'Status: %s, body=%s' % (response.status_code, response.text)
        return response

    def _send_request(self, path, api_key=None,
                      auth=None, data=None, method=None, userHeaders = {}):
        """High level sending request test.
        Do Negative tests if endpoints is enabled.
        If auth is required, send it without, verify 401 response.
        If api_key is required, send it without, verify 401 response.
        """
        if FLAGS.endpoints:
            if auth:
                print 'Negative test: remove auth.'
                r = self._call_http(path, api_key, None, data, method,
                                    userHeaders=userHeaders)
                self.assertEqual(r.status_code, 401)
                self.assertEqual(
                    r.json()['message'],
                    'JWT validation failed: Missing or invalid credentials')
                print 'Completed Negative test.'
            if api_key:
                print 'Negative test: remove api_key.'
                r = self._call_http(path, None, auth, data, method,
                                    userHeaders=userHeaders)
                self.assertEqual(r.status_code, 401)
                self.assertEqual(
                    r.json()['message'],
                    ('Method doesn\'t allow unregistered callers (callers without '
                     'established identity). Please use API Key or other form of '
                     'API consumer identity to call this API.'))
                print 'Completed Negative test.'
            return self._call_http(path, api_key, auth, data, method,
                                   userHeaders=userHeaders)
        else:
            return self._call_http(path, method=method, userHeaders=userHeaders)

    def verify_key_restriction(self):
        # ignore test if required informations are not provided
        if FLAGS.key_restriction_tests == None or \
          FLAGS.key_restriction_keys_file == None:
            return

        # check file exists
        if os.path.exists(FLAGS.key_restriction_tests) == False:
            logging.error("API keys restriction tests template not exist. " +
                          err.strerror)
            sys.exit(3)
        if os.path.exists(FLAGS.key_restriction_keys_file) == False:
            logging.error("API keys restriction key file not exist. " +
                          err.strerror)
            sys.exit(3)

        # load api keys
        with open(FLAGS.key_restriction_keys_file) as data_file:
            api_keys = json.load(data_file)

        with open(FLAGS.key_restriction_tests) as data_file:
            # Load template and render
            data_text = data_file.read();
            data_text = data_text.replace('${api_key_ip}', api_keys['ip']);
            data_text = data_text.replace('${api_key_ios}', api_keys['ios']);
            data_text = data_text.replace('${api_key_android}',
                                          api_keys['android']);
            data_text = data_text.replace('${api_key_referrers}',
                                          api_keys['referrers']);
            data = json.loads(data_text)
            # run test cases
            for type, testcases in data.iteritems():
                for testcase in testcases:
                    response = self._call_http(
                        testcase['path'],
                        api_key=testcase['api_key'],
                        userHeaders=testcase['headers'])
                    self.assertEqual(response.status_code,
                        testcase['status_code'])

    def run_all_tests(self):

        self.verify_key_restriction();

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
    parser.add_argument('--endpoints', type=bool,
            default=True, help='Is endpoints enabled on the backend?')
    parser.add_argument('--allow_unverified_cert', type=bool,
            default=False, help='used for testing self-signed ssl cert.')
    parser.add_argument('--key_restriction_tests',
                        help='Test suites for api key restriction.')
    parser.add_argument('--key_restriction_keys_file',
                        help='File contains API keys with restrictions ')
    flags = parser.parse_args(namespace=FLAGS)

    esp_test = EspBookstoreTest()
    try:
        esp_test.run_all_tests()
    except KeyError as e:
        sys.exit(esp_utils.red('Test failed.'))
