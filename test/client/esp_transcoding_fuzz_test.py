#!/usr/bin/python -u
#
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

""" Fuzz tests for transcoding. Uses generated JSON, URL path and query parameters input
    to call ESP with the grpc echo backend.

    Usage:
       ./esp_fuzz_test.py \
           --address="<ESP HTTP/1.1 address>" \
           --status_address="<ESP status address>" \
           [--runs=<number of runs>]
"""

import argparse
import esp_utils
import httplib
import json
import sys

class C:
    pass
FLAGS = C

class Fuzzer(object):
    """ A fuzzer based on templates and tokens. Each template is a string with
        one or more placeholders ('FUZZ'). The placeholders are replaced with tokens
        from the given token set to generate fuzz inputs.
    """

    def __init__(self, templates, tokens):
      self._templates = templates
      self._tokens = tokens

    def _run(self, template, f):
      if 'FUZZ' in template:
        for token in self._tokens:
          # Replace the first placeholder with the current token and call _run() again
          self._run(template.replace('FUZZ', token, 1), f)
      else:
        # All placeholders are already replaced, call the function f()
        f(template)

    def run(self, f):
      """ Executes the given function f() on all the generated fuzz inputs. """
      for template in self._templates:
        self._run(template, f)

class JsonFuzzer(Fuzzer):
    """ JSON fuzzer that uses static and generated templates """

    def __init__(self, max_object_nest_level, max_list_nest_level):
      STATIC_JSON_TEMPLATES = ['', 'FUZZ', '{FUZZ}', '[FUZZ]', '{FUZZ: FUZZ}', '[FUZZ, FUZZ]' ]
      JSON_TOKEN_LIST = ['{', '}', '[', ']', '"message"', '"text"', 'true', 'false', '1234',
                          '-1234', '1.2345e-10', '-3.1415e+13', 'null']

      templates = STATIC_JSON_TEMPLATES
      for n in range(max_object_nest_level):
        templates += [ self._get_object_template(n) ]
      for n in range(max_list_nest_level):
        templates += [ self._get_list_template(n) ]

      Fuzzer.__init__(self, templates, JSON_TOKEN_LIST)

    def _get_object_template(self, nest_level):
        template = ""
        for i in range(nest_level):
            template += '{ "message": FUZZ'
            if i != nest_level - 1:
                template += ', '
        for i in range(nest_level):
            template += '}'
        return template

    def _get_list_template(self, nest_level):
        template = ""
        for i in range(nest_level):
            template += '[FUZZ'
            if i != nest_level - 1:
                template += ', '
        for i in range(nest_level):
            template += ']'
        return template

def url_path_fuzzer():
    """ URL Path fuzzer that uses static templates """
    URLPATH_TEMPLATES = ['/FUZZ', '/FUZZ/FUZZ', '/FUZZ/FUZZ/FUZZ']
    URLPATH_TOKEN_LIST = ['', '/', '@$%^& ', '%20%25', 'echo', 'echostream']
    return Fuzzer(URLPATH_TEMPLATES, URLPATH_TOKEN_LIST)

def query_param_fuzzer():
    """ URL Path fuzzer that uses static templates """
    QUERYPARAM_TEMPLATES = ['', '#', '#FUZZ', 'FUZZ', 'FUZZ=FUZZ', 'FUZZ&FUZZ']
    QUERYPARAM_TOKEN_LIST = ['', '/', '#', '=', '&', '?', '&?#', '@#$%^=', 'message',
                              'message=msg', '%2F%25%0A%20' ]
    return Fuzzer(QUERYPARAM_TEMPLATES, QUERYPARAM_TOKEN_LIST)

class EspTranscodingFuzzTest(object):
    """ ESP Transcoding Fuzz tests """

    def __init__(self):
        self._conn = esp_utils.http_connection(FLAGS.address, True)
        self._status_conn = esp_utils.http_connection(FLAGS.status_address, True)
        self._initial_status = self._get_status()
        self._total_requests = 0
        self._unexpected_errors = 0

    def _get_status(self):
        self._status_conn.request('GET', '/endpoints_status')
        response = esp_utils.Response(self._status_conn.getresponse())
        status = response.json()
        if len(status['processes']) == 0:
            sys.exit(esp_utils.red('Cloud not get ESP status'))
        return status

    def _check_for_crash(self):
        print "Checking for crashes..."
        current_status = self._get_status()
        initial_processes = self._initial_status['processes']
        current_processes = current_status['processes']
        if len(initial_processes) != len(current_processes):
            sys.exit(esp_utils.red(
                'ESP crashed. Initial & current process numbers do not match.'))
            return
        for i in range(len(initial_processes)):
            if initial_processes[i]['processId'] != current_processes[i]['processId']:
                sys.exit(esp_utils.red(
                    'ESP crashed. Initial & current pids do not match.'))
            if initial_processes[i]['startedAt'] != current_processes[i]['startedAt']:
                sys.exit(esp_utils.red(
                    'ESP crashed. Initial & current start times do not match.'))
        print esp_utils.green('No crashes detected.')

    def _request(self, url_path, query_params, json, expected_codes, json_response):
        # Construct the URL using url_path, query_params and the api key
        url = url_path
        if FLAGS.api_key:
            if query_params:
                url = '%s?key=%s&%s' % (url_path, FLAGS.api_key, query_params)
            else:
                url = '%s?key=%s' % (url_path, FLAGS.api_key)
        elif query_params:
            url = '%s?%s' % (url_path, query_params)

        # Prepare headers
        headers = {'Content-Type': 'application/json'}
        if FLAGS.auth_token:
            headers['Authorization'] = 'Bearer ' + FLAGS.auth_token

        self._conn.request('POST', url, json, headers)
        response = esp_utils.Response(self._conn.getresponse())

        # Make sure that the response status code is in 200s, 300s or 400s
        # and that the response payload is valid JSON
        if not response.status_code in expected_codes:
            print esp_utils.red('Invalid status code %d: url=%s, json=%s' % (
                                response.status_code, url, json) )
            self._unexpected_errors += 1
        elif json_response and not response.is_json():
            print esp_utils.red('Response is not json: url=%s, json=%s' % (url, json) )
            self._unexpected_errors += 1

        self._total_requests += 1

    def _print_results_so_far(self):
        print 'Fuzz test results so far: total - %d, unexpected errors - %d' % (
            self._total_requests, self._unexpected_errors)

    def _run_json_fuzz_tests(self, url, max_object_nest_level, max_list_nest_level):
        fuzzer = JsonFuzzer(max_object_nest_level, max_list_nest_level)
        fuzzer.run(lambda json: self._request(url, None, json, [200, 400], True))

    def _run_query_param_fuzzer(self, url_path):
        fuzzer = query_param_fuzzer()
        fuzzer.run(
            lambda query_params: self._request(url_path, query_params, "{}", [200, 400], True))

    def _run_url_path_fuzzer(self):
        fuzzer = url_path_fuzzer()
        # Url path fuzzer generated requests may return 404 Not Found in
        # addition to 200 OK and 400 Bad Request
        fuzzer.run(lambda url_path: self._request(url_path, None, "{}", [200, 400, 404], False))

    def _run_fuzz_tests(self):
        print 'Running /echo JSON fuzz tests...'
        self._run_json_fuzz_tests('/echo', 4, 2)
        self._check_for_crash()
        self._print_results_so_far()

        print 'Running /echostream JSON fuzz tests...'
        self._run_json_fuzz_tests('/echostream', 2, 4)
        self._check_for_crash()
        self._print_results_so_far()

        print 'Running /echo query param fuzz tests...'
        self._run_query_param_fuzzer('/echo')
        self._check_for_crash()
        self._print_results_so_far()

        print 'Running URL path fuzz tests...'
        self._run_url_path_fuzzer()
        self._check_for_crash()
        self._print_results_so_far()

    def run_all_tests(self):
        for i in range(FLAGS.runs):
            self._run_fuzz_tests()
        if self._unexpected_errors > 0:
          sys.exit(esp_utils.red('Fuzz test failed.'))
        else:
          print esp_utils.green('Fuzz test passed.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--address', help='Deployed ESP HTTP/1.1 address.')
    parser.add_argument('--status_address',
                        help='Address for getting ESP status (/endpoints_status)')
    parser.add_argument('--api_key', help='Project api_key to access service.')
    parser.add_argument('--auth_token', help='Auth token.')
    parser.add_argument('--runs', type=int, default=1, help='Number of runs')
    flags = parser.parse_args(namespace=FLAGS)

    esp_test = EspTranscodingFuzzTest()
    esp_test.run_all_tests()
