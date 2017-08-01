#!/usr/bin/env python
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

import sys
import chilkat   #  Chilkat v9.5.0.66 or later.
import jwt  # pip install PyJWT and pip install cryptography.

""" This script is used to generate ES256-signed jwt token and public jwk. Before
    running, make sure to generate ES256 public/private key pair via following two
    command-line openssl commands.

    Create private key:
    $ openssl ecparam -genkey -name prime256v1 -noout -out myprivatekey.pem
    Create public key:
    $ openssl ec -in myprivatekey.pem -pubout -out mypubkey.pem
"""

def jwt_and_jwk_generator():
  # JWT token generation.
  with open('myprivatekey.pem', 'r') as f:
    try:
      secret = f.read()
    except:
      print("Private key file not present. Need to be generated.")
      sys.exit()
  # Change claim and headers field to fit needs.
  jwt_token = jwt.encode({'iss': '628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com',
                          'sub': '628645741881-noabiu23f5a8m8ovd8ucv698lj78vv0l@developer.gserviceaccount.com',
                          'aud': 'http://myservice.com/myapi'}, secret, algorithm="ES256",
                         headers={'alg': 'ES256', 'typ': 'JWT', 'kid': '1a'})
  print("ES256-signed jwt token:")
  print(jwt_token)

  #  Load public key file into memory.
  sbPem = chilkat.CkStringBuilder()
  success = sbPem.LoadFile("mypubkey.pem", "utf-8")
  if (success != True):
    print("Failed to load PEM file.")
    sys.exit()

  #  Load the key file into a public key object.
  pubKey = chilkat.CkPublicKey()
  success = pubKey.LoadFromString(sbPem.getAsString())
  if (success != True):
    print(pubKey.lastErrorText())
    sys.exit()
  #  Get the public key in JWK format:
  jwk = pubKey.getJwk()
  # Convert it to json format.
  json = chilkat.CkJsonObject()
  json.Load(jwk)
  # This line is used to set output format.
  json.put_EmitCompact(False)
  # Additional information can be added like this. change to fit needs.
  json.AppendString("alg","ES256")
  json.AppendString("kid","1a")
  # Print.
  print("Generated jwk:")
  print(json.emit())


if __name__ == '__main__':
  jwt_and_jwk_generator()
