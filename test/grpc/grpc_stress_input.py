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

"""
This script will generate input parameters in json format for
grpc-test-client to run stress test.

Its usage is:

  grpc_stress_input parameters | grpc-test-client

This test is using grpc-test.yaml service config. Backend supports two
methods; Echo and EchoStream. Echo doesn't require api-key nor auth.
EchoStream requires both api-key and auth.
"""

import gflags as flags
import json
import sys


FLAGS = flags.FLAGS

flags.DEFINE_string('server', 'localhost:8001', 'grpc server address')

flags.DEFINE_string('api_key', '', 'api_key for the project')

flags.DEFINE_string('auth_token', '', 'JWT token for auth')

flags.DEFINE_string('payload_string', 'Hollow World!', 'request payload string')

# This flag overrides "--payload_string" flag.
flags.DEFINE_string('payload_file', '', 'the file name to read the request payload')
# If larger than 0, randomly generates request payload.
# The playload size is random between 0 and value of this flag.
# This flag overrides all other payload related flags.
flags.DEFINE_integer('random_payload_max_size', 0,
                     'randomly generate request payload up to this size')

flags.DEFINE_integer('request_count', 10000,
                     'total number of requests to send')

flags.DEFINE_integer('concurrent', 10, 'The concurrent requests to send')

flags.DEFINE_float('allowed_failure_rate', 0.001, 'Allowed failure rate.')

flags.DEFINE_integer('requests_per_stream', 100,
                     'The number of requests for each stream')

kNoApiKeyError = ('Method doesn\'t allow unregistered callers (callers without'
                  ' established identity). Please use API Key or other form of'
                  ' API consumer identity to call this API.')


def GetRequest():
  if FLAGS.random_payload_max_size:
    return {
        'random_payload_max_size': FLAGS.random_payload_max_size
    }
  elif FLAGS.payload_file:
    return {
        'text': open(FLAGS.payload_file, 'r').read()
    }
  else:
    return {
        'text': FLAGS.payload_string
    }

def SubtestEcho():
  return {
      'weight': 1,
      'echo': {
          'request': GetRequest()
      }
  }

def SubtestEchoStream():
  return {
      'weight': 1,
      'echo_stream': {
          'request': GetRequest(),
          'count': FLAGS.requests_per_stream,
          'call_config': {
            'api_key': FLAGS.api_key,
            'auth_token': FLAGS.auth_token,
          },
      }
  }

def SubtestEchoStreamAuthFail():
  return {
      'weight': 1,
      'echo_stream': {
          'request': GetRequest(),
          'count': FLAGS.requests_per_stream,
          # Requires auth token.
          'expected_status': {
            'code': 16,
            'details': "JWT validation failed: Missing or invalid credentials",
          },
      }
  }

def SubtestEchoStreamNoApiKey():
  return {
      'weight': 1,
      'echo_stream': {
          'request': GetRequest(),
          'count': FLAGS.requests_per_stream,
          # Even auth check passed, it still requires api-key
          'call_config': {
             'auth_token': FLAGS.auth_token,
          },
          'expected_status': {
            'code': 16,
            'details': kNoApiKeyError,
          },
      }
  }


if __name__ == "__main__":
    try:
        argv = FLAGS(sys.argv)  # parse flags
    except flags.FlagsError as e:
        sys.exit('%s\nUsage: %s ARGS\n%s' % (e, sys.argv[0], FLAGS))

    print json.dumps({
        'server_addr': FLAGS.server,
        'plans': [{
            'parallel': {
              'test_count': FLAGS.request_count,
              'parallel_limit': FLAGS.concurrent,
              'allowed_failure_rate': FLAGS.allowed_failure_rate,
              'subtests': [
                SubtestEcho(),
                SubtestEchoStream(),
                SubtestEchoStreamAuthFail(),
                SubtestEchoStreamNoApiKey(),
              ],
            },
        }]
     }, indent=4)
