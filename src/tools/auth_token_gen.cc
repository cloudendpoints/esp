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
#include <memory>
#include <string>

#include "contrib/endpoints/src/api_manager/auth/lib/auth_token.h"

namespace {

const int MAX_JSON_SIZE = 10240;

const char test_service_control_audience[] =
    "https://test-servicecontrol.sandbox.googleapis.com/"
    "google.api.servicecontrol.v1.ServiceController";
const char prod_service_control_audience[] =
    "https://servicecontrol.sandbox.googleapis.com/"
    "google.api.servicecontrol.v1.ServiceController";

}  //  namespace

void print_usage() {
  fprintf(
      stderr,
      "Invalid argument.\n"
      "Usage: auth_token_gen json_secret_file [prod|test|audience_string].\n"
      "  json_secret_file: the client secret file from developer console.\n"
      "  prod: using production service control address.\n"
      "  test: (default) using test service control address.\n"
      "  audience_string: using user supplied audience.\n");
}

int main(int argc, char **argv) {
  if (argc <= 1) {
    print_usage();
    return 1;
  }
  printf("==Reading json file: %s\n", argv[1]);
  FILE *fp = fopen(argv[1], "r");
  char json_string[MAX_JSON_SIZE];
  size_t json_string_len = fread(json_string, 1, MAX_JSON_SIZE, fp);
  fclose(fp);
  json_string[json_string_len] = 0;

  std::string audience = test_service_control_audience;
  if (argc > 2) {
    if (strcmp(argv[2], "prod") == 0) {
      audience = prod_service_control_audience;
    } else if (strcmp(argv[2], "test") != 0) {
      audience = argv[2];
    }
  }

  printf("==Parsing json string: %s\n", json_string);
  printf("==Use audience: %s\n", audience.c_str());
  char *token = ::google::api_manager::auth::esp_get_auth_token(
      json_string, audience.c_str());
  if (token) {
    printf("==Auth token: %s\n", token);
    ::google::api_manager::auth::esp_grpc_free(token);
  } else {
    fprintf(stderr, "Error: failed to encode and sign.\n");
  }
  return 0;
}
