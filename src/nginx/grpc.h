/*
 * Copyright (C) Extensible Service Proxy Authors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef NGINX_NGX_ESP_GRPC_H_
#define NGINX_NGX_ESP_GRPC_H_

#include <memory>
#include <vector>

extern "C" {
#include "ngx_core.h"
#include "src/http/ngx_http.h"
}

namespace google {
namespace api_manager {
namespace nginx {

// Configures a location to use the GRPC-aware proxy backend handler.
char *ConfigureGrpcBackendHandler(ngx_conf_t *ct, ngx_command_t *cmd,
                                  void *conf);

// Checks content type to see if it is a Grpc request.
bool IsGrpcRequest(ngx_http_request_t *r);

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // NGINX_NGX_ESP_GRPC_H_
