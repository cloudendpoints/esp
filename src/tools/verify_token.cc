
// Copyright (C) Extensible Service Proxy Authors
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
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <memory>
#include <string>

#include "src/api_manager/auth/lib/auth_jwt_validator.h"
#include "src/api_manager/auth/lib/auth_token.h"

namespace {

const int MAX_BUF_SIZE = 10240;

}  //  namespace

void print_usage() {
  std::cerr << "Invalid argument.\n"
               "Usage: verify_token token_file public_key_file\n";
}

std::string read_file(const char *file) {
  std::cerr << "==Reading file: " << file << std::endl;
  FILE *fp = fopen(file, "r");
  char buf[MAX_BUF_SIZE];
  size_t buf_len = fread(buf, 1, sizeof(buf), fp);
  fclose(fp);
  buf[buf_len] = 0;
  return std::string(buf, buf_len);
}

int main(int argc, char **argv) {
  if (argc <= 2) {
    print_usage();
    return 1;
  }
  //  ::google::api_manager::auth::esp_init_grpc_log();

  std::string token = read_file(argv[1]);
  std::string jwks = read_file(argv[2]);

  std::cerr << "Token:" << token << std::endl;
  std::cerr << "Jwks:" << jwks << std::endl;

  auto validator = ::google::api_manager::auth::JwtValidator::Create(
      token.c_str(), token.size());
  ::google::api_manager::UserInfo user_info;
  auto status1 = validator->Parse(&user_info);
  std::cerr << "Parse result: " << status1.ToString() << std::endl;

  auto status2 = validator->VerifySignature(jwks.c_str(), jwks.size());
  std::cerr << "Varify result: " << status2.ToString() << std::endl;
  return 0;
}
