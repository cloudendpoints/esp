// Copyright (C) Endpoints Server Proxy Authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//
////////////////////////////////////////////////////////////////////////////////
//
package utils

const (
  OK = 0
  CANCELLED = 1
  UNKNOWN = 2
  INVALID_ARGUMENT = 3
  DEADLINE_EXCEEDED = 4
  NOT_FOUND = 5
  ALREADY_EXISTS = 6
  PERMISSION_DENIED = 7
  UNAUTHENTICATED = 16
  RESOURCE_EXHAUSTED = 8
  FAILED_PRECONDITION = 9
  ABORTED = 10
  OUT_OF_RANGE = 11
  UNIMPLEMENTED = 12
  INTERNAL = 13
  UNAVAILABLE = 14
  DATA_LOSS = 15
)

func HttpResponseCodeToStatusCode(code int) int {
	switch {
	case code == 400: return INVALID_ARGUMENT
	case code == 401: return UNAUTHENTICATED
	case code == 403: return PERMISSION_DENIED
	case code == 404: return NOT_FOUND
	case code == 409: return ABORTED
	case code == 416: return OUT_OF_RANGE
	case code == 429: return RESOURCE_EXHAUSTED
	case code == 499: return CANCELLED
	case code == 501: return UNIMPLEMENTED
	case code == 503: return UNAVAILABLE
	case code == 504: return DEADLINE_EXCEEDED
	case code >= 200 && code < 300: return OK
	case code >= 400 && code < 500: return FAILED_PRECONDITION
	case code >= 500 && code < 600: return INTERNAL
	}
	return UNKNOWN
}
