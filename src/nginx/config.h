/*
 * Copyright (C) Endpoints Server Proxy Authors
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
#ifndef NGINX_NGX_ESP_CONFIG_H_
#define NGINX_NGX_ESP_CONFIG_H_

extern "C" {
#include "third_party/nginx/src/http/ngx_http.h"
}

namespace google {
namespace api_manager {
namespace nginx {

// Parses endpoints block.
char *ngx_http_endpoints_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

// Sets endpoints status handler.
char *ngx_esp_configure_status_handler(ngx_conf_t *cf, ngx_command_t *cmd,
                                       void *conf);

// Config loading utility functions.

// Reads the whole file into a memory block allocated from the pool.
ngx_int_t ngx_esp_read_file(const char *filename, ngx_pool_t *pool,
                            ngx_str_t *data);

// Reads the whole file into a memory block allocated from the pool.
// Allocates one additional char of memory and '\0'-terminates the
// data read from the file.
ngx_int_t ngx_esp_read_file_null_terminate(const char *filename,
                                           ngx_pool_t *pool, ngx_str_t *data);

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // NGINX_NGX_ESP_CONFIG_H_
