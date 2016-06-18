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
#
################################################################################
#
# Custom Skylark rules for basic Perl library and test support.

_perl_file_types = FileType([
    ".t",
    ".pm",
])

_perl_srcs_attr = attr.label_list(allow_files = _perl_file_types)

_perl_deps_attr = attr.label_list(
    allow_files = False,
    providers = ["transitive_perl_sources"],
)

_perl_data_attr = attr.label_list(
    cfg = DATA_CFG,
    allow_files = True,
)

_perl_main_attr = attr.label(
    allow_files = _perl_file_types,
    single_file = True,
)

_perl_env_attr = attr.string_dict()

def _collect_transitive_sources(ctx):
  result = set(order="compile")
  for dep in ctx.attr.deps:
    result += dep.transitive_perl_sources

  result += _perl_file_types.filter(ctx.files.srcs)
  return result

def _get_main_from_sources(ctx):
  sources = _perl_file_types.filter(ctx.files.srcs)
  if len(sources) != 1:
    fail("Cannot infer main from multiple 'srcs'. Please specify 'main' attribute.", "main")
  return sources[0]

def _perl_library_implementation(ctx):
  transitive_sources = _collect_transitive_sources(ctx)
  return struct(
    runfiles = ctx.runfiles(collect_data = True),
    transitive_perl_sources = transitive_sources
  )

def _is_identifier(name):
  # Must be non-empty.
  if name == None or len(name) == 0:
    return False
  # Must start with alpha or '_'
  if not name[0].isalpha() and name[0] != '_':
    return False
  # Must consist of alnum characters or '_'s.
  for c in name:
    if not c.isalnum() and c != '_':
      return False
  return True

def _perl_test_implementation(ctx):
  transitive_sources = _collect_transitive_sources(ctx)

  main = ctx.file.main
  if main == None:
    main = _get_main_from_sources(ctx)

  env = ""
  for name, value in ctx.attr.env.items():
    if not _is_identifier(name):
      fail("%s is not a valid environment variable name." % str(name))
    env += "%s=\"%s\" \\\n" % (name, value.replace("\"", "\\\""))

  ctx.file_action(
    output=ctx.outputs.executable,
    content="""#!/bin/bash
if [[ "%s" != $(basename "${PWD}") ]]; then
  DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
  cd "${DIR}/%s.runfiles" || { echo "Cannot find runfiles in ${DIR}." ; exit 1 ; }
fi
TEST_SRCDIR="${TEST_SRCDIR:-$PWD}/%s"
%s perl "${TEST_SRCDIR}/%s"
exit $?
""" % (ctx.workspace_name,
       ctx.outputs.executable.basename,
       ctx.workspace_name,
       env,
       main.path),
    executable=True
  )

  return struct(
      files = set([ctx.outputs.executable]),
      runfiles=ctx.runfiles(
          collect_data=True,
          collect_default=True,
          transitive_files=transitive_sources + [ctx.outputs.executable],
      )
  )

perl_library = rule(
    attrs = {
        "srcs": _perl_srcs_attr,
        "deps": _perl_deps_attr,
        "data": _perl_data_attr,
    },
    implementation = _perl_library_implementation,
)

perl_test = rule(
    attrs = {
        "srcs": _perl_srcs_attr,
        "deps": _perl_deps_attr,
        "data": _perl_data_attr,
        "main": _perl_main_attr,
        "env": _perl_env_attr,
    },
    executable = True,
    test = True,
    implementation = _perl_test_implementation,
)
