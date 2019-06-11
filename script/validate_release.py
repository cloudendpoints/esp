#!/usr/bin/python
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
#
################################################################################
"""Look at the json files extracted and validate git commit."""

import collections
import fnmatch
import gflags
import json
import logging
import os
import prettytable
import sys

from datetime import date

COMMIT = 'headCommitHash'
DATE = 'date'
RUN_ID = 'runId'
STATUS = 'scriptStatus'
TEST_ID = 'testId'

# List of test that are expected to have run successfully at least once.
RUN_TESTS = [
    'asan',
    'check-files',
    'docker-tests',
    'flex-on-endpoints-on',
    'gke-custom-http-bookstore',
    'gke-loose-http-bookstore',
    'gke-loose-https-bookstore',
    'gke-tight-http-bookstore',
    'gke-tight-http2-echo',
    'gke-tight-http2-interop',
    'gke-tight-https-bookstore',
    'jenkins-perf-test-vm-esp',
    'jenkins-post-submit-perf-test',
    'long-run-test_flex-on-endpoints-on',
    'long-run-test_gke-tight-http2-echo',
    'long-run-test_gke-tight-http2-interop',
    'long-run-test_gke-tight-https-bookstore',
    'release',
]

FLAGS = gflags.FLAGS


gflags.DEFINE_string(
    'commit_sha', '',
    'The expected git commit sha'
)
gflags.DEFINE_string(
    'path',
    '',
    'Path where all release information has been extracted'
)
gflags.DEFINE_boolean(
    'detail',
    False,
    'Prints detailled output.'
)

class Error(Exception):
  """Base Error for this class"""


class JsonParsingError(Error):
  """Error for Json parsing."""


class CommitError(Error):
  """Commit found is not what was expected."""


def findFiles(path, pattern):
  """Find file in a directory with a given pattern."""
  for root, dirs, files in os.walk(path):
    for file in files:
      if fnmatch.fnmatch(file, pattern):
        yield os.path.join(root, file)


class ReleaseValidation(object):
  """Check Release."""

  def __init__(self, reference_commit):
    self._ref_commit = reference_commit
    self._test_info = collections.defaultdict(dict)
    self._successful_test = set()

  def AddJsonData(self, json_file):
    """Parse Json file and add test info."""
    logging.info('Adding information from %s' % json_file)
    try:
      with open(json_file) as log:
        result = json.load(log)
    except:
      raise JsonParsingError('Unable to parse %s' % json_file)

    status = result.get(STATUS, '')
    test_id = result.get(TEST_ID, '')
    run_id = result.get(RUN_ID, '')
    commit = result.get(COMMIT, '')
    unix_timestamp = result.get(DATE, '')

    if '' in [commit, run_id, status, test_id]:
      raise JsonParsingError('Unable to parse %s' % json_file)

    if commit != self._ref_commit:
      raise CommitError('%s != %s' % (commit, self._ref_commit))

    self._test_info[test_id][run_id] = {
        DATE: date.fromtimestamp(float(unix_timestamp)),
        STATUS: int(status)
    }

    if int(status) == 0:
      self._successful_test.add(test_id)

  def ExtractTestInfoFromPath(self, path):
    """Extracts test information for a path."""
    for json_file in findFiles(path, '*.json'):
      try:
        self.AddJsonData(json_file)
      except Error:
        logging.error('Could not parse %s', json_file)

  def PrintAllTests(self):
    """Prints all test information."""
    if not self._test_info:
      return
    table = prettytable.PrettyTable(
        [TEST_ID, RUN_ID, DATE, STATUS])

    for test_id, run in self._test_info.iteritems():
      for run_id, values in run.iteritems():
        date = values[DATE]
        status = values[STATUS]
        table.add_row([test_id, run_id, date, status])

    table.align = 'l'
    table.sortby = TEST_ID
    print table

  def PrintSummary(self):
    """Prints tests summary."""
    if not self._test_info:
      return
    table = prettytable.PrettyTable(
        [TEST_ID, 'Success', 'Failure'])

    for test_id, run in self._test_info.iteritems():
      failures, successes = 0, 0
      for values in run.values():
        status = values[STATUS]
        if status == 0:
          successes += 1
        else:
          failures +=1
      table.add_row([test_id, successes, failures])

    table.align = 'l'
    table.sortby = TEST_ID
    print table

  def ValidateRelease(self):
    missing_tests = set(RUN_TESTS).difference(self._successful_test)
    if missing_tests:
      logging.error(
          'The following tests haven\'t been run successfully once: \n - %s',
          '\n - '.join(sorted(missing_tests)))
    return len(missing_tests)


def main(unused_argv):
  releaseVal = ReleaseValidation(FLAGS.commit_sha)
  releaseVal.ExtractTestInfoFromPath(FLAGS.path)
  if FLAGS.detail:
    releaseVal.PrintAllTests()
  else:
    releaseVal.PrintSummary()
  sys.exit(releaseVal.ValidateRelease())


if __name__ == '__main__':
  logging.basicConfig(stream=sys.stdout, level=logging.ERROR)
  try:
    argv = FLAGS(sys.argv) # Parse flags
  except:
    sys.exit('%s\nUsage: %s ARGS\n%s' % (e, sys.argv[0], FLAGS))

  if not FLAGS.commit_sha:
    sys.exit('Flag commit_sha is required')

  if not FLAGS.path:
    sys.exit('Flag path required.')

  main(argv)
