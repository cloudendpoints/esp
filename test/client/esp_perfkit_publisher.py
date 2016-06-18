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

import collections
import esp_utils
import getpass
import gflags as flags
import json
import tempfile
import time
import uuid


FLAGS = flags.FLAGS

flags.DEFINE_string(
    'product_name', 'ESP',
    'The product name to use when publishing results.')

flags.DEFINE_string('owner', getpass.getuser(), 'Owner name. '
                    'Used to tag created resources and performance records.')

flags.DEFINE_boolean(
    'official', False,
    'A boolean indicating whether results are official or not. The '
    'default is False. Official test results are treated and queried '
    'differently from non-official test results.')

flags.DEFINE_string(
    'bigquery_table', 'esp-test-dashboard:samples_mart.results',
    'The BigQuery table to publish results to. This should be of the form '
    '"[project_id:]dataset_name.table_name".')

flags.DEFINE_string(
    'bq_path', 'bq', 'Path to the "bq" executable.')

flags.DEFINE_boolean(
    'use_gce', True,
    'If script is running in GCE, use GCE cendentail.')


def GetLabelsFromDict(metadata):
    """Converts a metadata dictionary to a string of labels.
    Args:
      metadata: a dictionary of string key value pairs.
    Returns:
      A string of labels in the format that Perfkit uses.
    """
    return ','.join('|%s:%s|' % (k, v) for k, v in metadata.iteritems())


def WriteSamplesToFile(file_path, results, test_env):
    """Write samples to file."""
    print '==== Write %d samples to %s' % (len(results), file_path)
    spec = {
        'test': test_env['test'] if 'test' in test_env else 'unknown-test',
        'product_name': test_env['product_name'] if 'product_name' in test_env else FLAGS.product_name,
        'official': test_env['official'] if 'official' in test_env else FLAGS.official,
        'run_uri': test_env['run_id'] if 'run_id' in test_env else str(uuid.uuid4()),
        'log_uri': test_env['run_description'] if 'run_description' in test_env else str(uuid.uuid4()),
        'owner': test_env['owner'] if 'owner' in test_env else FLAGS.owner
    }
    print '==== Test spec: %s' % str(spec)
    with open(file_path, 'wb') as fp:
        for metrics, test_meta in results:
            for key, value in metrics.iteritems():
                meta = test_meta.copy()
                del meta['time']
                sample = {
                    'metric': key,
                    'value': value[0],
                    'unit': value[1],
                    'timestamp': test_meta['time'],
                    'sample_uri': str(uuid.uuid4()),
                    'labels': GetLabelsFromDict(meta)
                }
                sample.update(spec)
                fp.write(json.dumps(sample) + '\n')


def LoadToBigQuery(file_path):
    """Load the samples from a file to BigQuery."""
    print '==== Publishing samples from file %s to %s' % (file_path,
                                                          FLAGS.bigquery_table)
    load_cmd = [FLAGS.bq_path]
    if FLAGS.use_gce:
        load_cmd.append('--use_gce_service_account')
    load_cmd.extend(['load',
                     '--source_format=NEWLINE_DELIMITED_JSON',
                     FLAGS.bigquery_table, file_path,
                     'results_table_schema.json'])
    (out, ret) = esp_utils.IssueCommand(load_cmd)


def Publish(results, test_env):
    """Publish the result to Perfkit BigQuery table."""
    with tempfile.NamedTemporaryFile(prefix='esp-test-perfkit-bq',
                                     suffix='.json') as tf:
        WriteSamplesToFile(tf.name, results, test_env)
        LoadToBigQuery(tf.name)
        tf.close()
