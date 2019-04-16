#!/bin/bash
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

set -e

# $1=<CA name>
generate_ca() {
  openssl genrsa -out $1key.pem 2048
  openssl req -new -key $1key.pem -out $1cert.csr -config $1cert.cfg -batch -sha256
  openssl x509 -req -days 730 -in $1cert.csr -signkey $1key.pem -out $1cert.pem \
    -extensions v3_ca -extfile $1cert.cfg
}

# $1=<certificate name>
generate_rsa_key() {
  openssl genrsa -out $1key.pem 2048
}

# $1=<certificate name> $2=<CA name>
generate_x509_cert() {
  openssl req -new -key $1key.pem -out $1cert.csr -config $1cert.cfg -batch -sha256
  openssl x509 -req -days 730 -in $1cert.csr -sha256 -CA $2cert.pem -CAkey \
    $2key.pem -CAcreateserial -out $1cert.pem -extensions v3_ca -extfile $1cert.cfg
}

# Generate cert for the CA.
generate_ca ca
# Generate RSA cert for the server.
generate_rsa_key server ca
generate_x509_cert server ca

rm *.csr
rm *.srl
