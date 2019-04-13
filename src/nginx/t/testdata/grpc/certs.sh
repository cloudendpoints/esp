#!/bin/bash

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
