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
# A module to define Status code.

package Status;

# Status code copied from
# https://github.com/google/protobuf/blob/master/src/google/protobuf/stubs/status.h
my  %Code = (
  OK => 0,
  CANCELLED => 1,
  UNKNOWN => 2,
  INVALID_ARGUMENT => 3,
  DEADLINE_EXCEEDED => 4,
  NOT_FOUND => 5,
  ALREADY_EXISTS => 6,
  PERMISSION_DENIED => 7,
  UNAUTHENTICATED => 16,
  RESOURCE_EXHAUSTED => 8,
  FAILED_PRECONDITION => 9,
  ABORTED => 10,
  OUT_OF_RANGE => 11,
  UNIMPLEMENTED => 12,
  INTERNAL => 13,
  UNAVAILABLE => 14,
  DATA_LOSS => 15
);

sub http_response_code_to_status_code {
  my $response_code = shift;
  if ($response_code == 400) { return $Code{INVALID_ARGUMENT}; }
  if ($response_code == 401) { return $Code{UNAUTHENTICATED}; }
  if ($response_code == 403) { return $Code{PERMISSION_DENIED}; }
  if ($response_code == 404) { return $Code{NOT_FOUND}; }
  if ($response_code == 409) { return $Code{ABORTED}; }
  if ($response_code == 416) { return $Code{OUT_OF_RANGE}; }
  if ($response_code == 429) { return $Code{RESOURCE_EXHAUSTED}; }
  if ($response_code == 499) { return $Code{CANCELLED}; }
  if ($response_code == 504) { return $Code{DEADLINE_EXCEEDED}; }
  if ($response_code == 501) { return $Code{UNIMPLEMENTED}; }
  if ($response_code == 503) { return $Code{UNAVAILABLE}; }
  if ($response_code >= 200 && $response_code < 300) { return $Code{OK}; }
  if ($response_code >= 400 && $response_code < 500) { return $Code{FAILED_PRECONDITION}; }
  if ($response_code >= 500 && $response_code < 600) { return $Code{INTERNAL}; }
  return $Code{UNKNOWN};
}

