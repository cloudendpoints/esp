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
# A shared module for ESP auth end-to-end tests.

package Auth;

use strict;
use warnings;

use ApiManager;

sub get_auth_token {
  my ($secret_file, $audience) = @_;
  my $cmd = $ENV{TEST_SRCDIR}."/src/tools/auth_token_gen $secret_file ";
  if (!defined $audience) {
    # Defaults to service name.
    $audience = "endpoints-test.cloudendpointsapis.com";
  }
  $cmd .= $audience;
  open(RES, "$cmd|") || die "Failed: $!\n";
  while (<RES>) {
    if (/Auth token/) {
      chomp;
      my ($key, $value) = split /: /;
      return $value;
    }
  }
  die "Failed: can not retrieve token for " . $secret_file;
}

sub get_public_key_jwk {
  return <<'EOF';
{
 "keys": [
  {
   "kty": "RSA",
   "alg": "RS256",
   "use": "sig",
   "kid": "62a93512c9ee4c7f8067b5a216dade2763d32a47",
   "n": "0YWnm_eplO9BFtXszMRQNL5UtZ8HJdTH2jK7vjs4XdLkPW7YBkkm_2xNgcaVpkW0VT2l4mU3KftR-6s3Oa5Rnz5BrWEUkCTVVolR7VYksfqIB2I_x5yZHdOiomMTcm3DheUUCgbJRv5OKRnNqszA4xHn3tA3Ry8VO3X7BgKZYAUh9fyZTFLlkeAh0-bLK5zvqCmKW5QgDIXSxUTJxPjZCgfx1vmAfGqaJb-nvmrORXQ6L284c73DUL7mnt6wj3H6tVqPKA27j56N0TB1Hfx4ja6Slr8S4EB3F1luYhATa1PKUSH8mYDW11HolzZmTQpRoLV8ZoHbHEaTfqX_aYahIw",
   "e": "AQAB"
  },
  {
   "kty": "RSA",
   "alg": "RS256",
   "use": "sig",
   "kid": "b3319a147514df7ee5e4bcdee51350cc890cc89e",
   "n": "qDi7Tx4DhNvPQsl1ofxxc2ePQFcs-L0mXYo6TGS64CY_2WmOtvYlcLNZjhuddZVV2X88m0MfwaSA16wE-RiKM9hqo5EY8BPXj57CMiYAyiHuQPp1yayjMgoE1P2jvp4eqF-BTillGJt5W5RuXti9uqfMtCQdagB8EC3MNRuU_KdeLgBy3lS3oo4LOYd-74kRBVZbk2wnmmb7IhP9OoLc1-7-9qU1uhpDxmE6JwBau0mDSwMnYDS4G_ML17dC-ZDtLd1i24STUw39KH0pcSdfFbL2NtEZdNeam1DDdk0iUtJSPZliUHJBI_pj8M-2Mn_oA8jBuI8YKwBqYkZCN1I95Q",
   "e": "AQAB"
  }
 ]
}
EOF
}

sub get_public_key_x509 {
  return <<'EOF';
{
 "62a93512c9ee4c7f8067b5a216dade2763d32a47": "-----BEGIN CERTIFICATE-----\nMIIDYDCCAkigAwIBAgIIEzRv3yOFGvcwDQYJKoZIhvcNAQEFBQAwUzFRME8GA1UE\nAxNINjI4NjQ1NzQxODgxLW5vYWJpdTIzZjVhOG04b3ZkOHVjdjY5OGxqNzh2djBs\nLmFwcHMuZ29vZ2xldXNlcmNvbnRlbnQuY29tMB4XDTE1MDkxMTIzNDg0OVoXDTI1\nMDkwODIzNDg0OVowUzFRME8GA1UEAxNINjI4NjQ1NzQxODgxLW5vYWJpdTIzZjVh\nOG04b3ZkOHVjdjY5OGxqNzh2djBsLmFwcHMuZ29vZ2xldXNlcmNvbnRlbnQuY29t\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0YWnm/eplO9BFtXszMRQ\nNL5UtZ8HJdTH2jK7vjs4XdLkPW7YBkkm/2xNgcaVpkW0VT2l4mU3KftR+6s3Oa5R\nnz5BrWEUkCTVVolR7VYksfqIB2I/x5yZHdOiomMTcm3DheUUCgbJRv5OKRnNqszA\n4xHn3tA3Ry8VO3X7BgKZYAUh9fyZTFLlkeAh0+bLK5zvqCmKW5QgDIXSxUTJxPjZ\nCgfx1vmAfGqaJb+nvmrORXQ6L284c73DUL7mnt6wj3H6tVqPKA27j56N0TB1Hfx4\nja6Slr8S4EB3F1luYhATa1PKUSH8mYDW11HolzZmTQpRoLV8ZoHbHEaTfqX/aYah\nIwIDAQABozgwNjAMBgNVHRMBAf8EAjAAMA4GA1UdDwEB/wQEAwIHgDAWBgNVHSUB\nAf8EDDAKBggrBgEFBQcDAjANBgkqhkiG9w0BAQUFAAOCAQEAP4gkDCrPMI27/QdN\nwW0mUSFeDuM8VOIdxu6d8kTHZiGa2h6nTz5E+twCdUuo6elGit3i5H93kFoaTpex\nj/eDNoULdrzh+cxNAbYXd8XgDx788/jm06qkwXd0I5s9KtzDo7xxuBCyGea2LlpM\n2HOI4qFunjPjFX5EFdaT/Rh+qafepTKrF/GQ7eGfWoFPbZ29Hs5y5zATJCDkstkY\npnAya8O8I+tfKjOkcra9nOhtck8BK94tm3bHPdL0OoqKynnoRCJzN5KPlSGqR/h9\nSMBZzGtDOzA2sX/8eyU6Rm4MV6/1/53+J6EIyarR5g3IK1dWmz/YT/YMCt6LhHTo\n3yfXqQ==\n-----END CERTIFICATE-----\n",
 "b3319a147514df7ee5e4bcdee51350cc890cc89e": "-----BEGIN CERTIFICATE-----\nMIIDYDCCAkigAwIBAgIICjE9gZxAlu8wDQYJKoZIhvcNAQEFBQAwUzFRME8GA1UE\nAxNINjI4NjQ1NzQxODgxLW5vYWJpdTIzZjVhOG04b3ZkOHVjdjY5OGxqNzh2djBs\nLmFwcHMuZ29vZ2xldXNlcmNvbnRlbnQuY29tMB4XDTE1MDkxMzAwNTAyM1oXDTI1\nMDkxMDAwNTAyM1owUzFRME8GA1UEAxNINjI4NjQ1NzQxODgxLW5vYWJpdTIzZjVh\nOG04b3ZkOHVjdjY5OGxqNzh2djBsLmFwcHMuZ29vZ2xldXNlcmNvbnRlbnQuY29t\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAqDi7Tx4DhNvPQsl1ofxx\nc2ePQFcs+L0mXYo6TGS64CY/2WmOtvYlcLNZjhuddZVV2X88m0MfwaSA16wE+RiK\nM9hqo5EY8BPXj57CMiYAyiHuQPp1yayjMgoE1P2jvp4eqF+BTillGJt5W5RuXti9\nuqfMtCQdagB8EC3MNRuU/KdeLgBy3lS3oo4LOYd+74kRBVZbk2wnmmb7IhP9OoLc\n1+7+9qU1uhpDxmE6JwBau0mDSwMnYDS4G/ML17dC+ZDtLd1i24STUw39KH0pcSdf\nFbL2NtEZdNeam1DDdk0iUtJSPZliUHJBI/pj8M+2Mn/oA8jBuI8YKwBqYkZCN1I9\n5QIDAQABozgwNjAMBgNVHRMBAf8EAjAAMA4GA1UdDwEB/wQEAwIHgDAWBgNVHSUB\nAf8EDDAKBggrBgEFBQcDAjANBgkqhkiG9w0BAQUFAAOCAQEAHSPR7fDAWyZ825IZ\n86hEsQZCvmC0QbSzy62XisM/uHUO75BRFIAvC+zZAePCcNo/nh6FtEM19wZpxLiK\n0m2nqDMpRdw3Qt6BNhjJMozTxA2Xdipnfq+fGpa+bMkVpnRZ53qAuwQpaKX6vagr\nj83Bdx2b5WPQCg6xrQWsf79Vjj2U1hdw7+klcF7tLef1p8qA/ezcNXmcZ4BpbpaO\nN9M4/kQOA3Y2F3ISAaOJzCB25F259whjW+Uuqd/L9Lb4gPPSUMSKy7Zy4Sn4il1U\nFc94Mi9j13oeGvLOduNOStGu5XROIxDtCEjjn2y2SL2bPw0qAlIzBeniiApkmYw/\no6OLrg==\n-----END CERTIFICATE-----\n"
}
EOF
}

1;
