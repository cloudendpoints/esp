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
"""Post Review to Gerrit for Presubmits."""

import cookielib
import gflags
import json
import logging
import os
import urllib2
import sys


FLAGS = gflags.FLAGS
GIT_COOKIES = os.path.join(
    os.environ.get('HOME'), '.git-credential-cache/cookie')
_RUN_FLOW = 'run'
_VERIFY_FLOW = 'verify'
_FLOWS = ['run']


gflags.DEFINE_string(
    'build_url', '',
    'Jenkins Build url.'
)
gflags.DEFINE_string(
    'change_id', '',
    'The change id we are reviewing.'
)
gflags.DEFINE_string(
    'commit', '',
    'The git commit we are reviewing.'
)
gflags.DEFINE_enum(
    'flow', None, [_RUN_FLOW, _VERIFY_FLOW],
    'Which flow to run.'
)
gflags.DEFINE_string(
    'gerrit_url', '',
    'Gerrit Review url.'
)
gflags.DEFINE_boolean(
    'success', False,
    'Whether presbumit passed or failed.'
)


class SimpleGerrit(object):

  def __init__(self, base_url, change_id, commit, cookie_file=GIT_COOKIES):
    self._change_id = change_id
    self._commit = commit
    self._base_url = base_url
    logging.info('Loading cookies from %s', cookie_file)
    cj = cookielib.MozillaCookieJar(cookie_file)
    cj.load()
    self._opener = urllib2.build_opener(urllib2.HTTPCookieProcessor(cj))

  def _SendRequest(self, url, data):
    request = urllib2.Request(url)
    request.add_header('Content-Type','application/json')
    try:
      response = self._opener.open(request, json.dumps(data))
      logging.info(
          'URL: %s, Code: %d, Info: \n%s',
          response.geturl(), response.getcode(), response.info())
    except urllib2.URLError:
      logging.exception("Could not reach %s", url)
      raise

  def SetReview(self, tag, message, labels=None):
    logging.info(
        'Setting review on change %s, commit %s',
        self._change_id, self._commit)
    url = '{base_url}/a/changes/{change_id}/revisions/{commit}/review'.format(
        base_url=self._base_url,
        change_id=self._change_id,
        commit=self._commit)
    data = {
        'tag': tag,
        'message': message
    }
    if labels:
      data['labels'] = labels
    self._SendRequest(url, data)


class JenkinsGerrit(object):

  def __init__(self, base_url, change_id, commit):
    self._gerrit = SimpleGerrit(base_url, change_id, commit)

  def RunPresubmit(self, build_url):
    self._gerrit.SetReview(
        tag='jenkins',
        message='Running presubmits at {build_url} ...'.format(
            build_url=build_url))

  def VerifyPresubmit(self, success, build_url):
    status = 'Successful' if success else 'Failed'
    verified = '+1' if success else '-1'
    message = (
        '{status} presubmits. '
        'Details at {build_url}').format(
      status=status,
      build_url=build_url)
    labels = {
        'Verified': verified
    }
    self._gerrit.SetReview(
        tag='jenkins',
        message=message,
        labels=labels)

if __name__ == '__main__':
  logging.basicConfig(stream=sys.stdout, level=logging.INFO)
  try:
    argv = FLAGS(sys.argv) # Parse flags
  except Exception as e:
    sys.exit('%s\nUsage: %s ARGS\n%s' % (e, sys.argv[0], FLAGS))

  if not FLAGS.commit:
    sys.exit('Flag commit is required')
  if not FLAGS.change_id:
    sys.exit('Flag change_id is required')
  if not FLAGS.gerrit_url:
    sys.exit('Flag gerrit_url is required')
  if not FLAGS.build_url:
    sys.exit('Flag build_url is required')

  client = JenkinsGerrit(
      base_url=FLAGS.gerrit_url,
      change_id=FLAGS.change_id,
      commit=FLAGS.commit)

  try:
    if FLAGS.flow == _RUN_FLOW:
      client.RunPresubmit(
          build_url=FLAGS.build_url)
    elif FLAGS.flow == _VERIFY_FLOW:
      client.VerifyPresubmit(
          success=FLAGS.success,
          build_url=FLAGS.build_url)
    else:
      sys.exit('Flag flow must be set')
  except:
    logging.exception('Could not update gerrit')
    sys.exit('Could not update gerrit')

