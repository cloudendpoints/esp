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
#ifndef NGINX_NGX_ESP_STATUS_H_
#define NGINX_NGX_ESP_STATUS_H_

#include <chrono>

#include "include/api_manager/api_manager.h"

extern "C" {
#include "src/http/ngx_http.h"
}

namespace google {
namespace api_manager {
namespace nginx {

// The maximum number of esp objects.
const int kMaxEspNum = 10;
const int kMaxServiceNameSize = 256;

typedef struct {
  // process ID
  ngx_pid_t pid;

  // process start time
  std::chrono::system_clock::time_point start_time;

  // maximum resident set size of process (peak memory usage)
  long maxrss;

  // current resident set size of process (current memory usage)
  long current_rss;

  // virtual size of the process (current memory usage)
  long virtual_size;

  // user CPU time used
  std::chrono::microseconds user_time;

  // system CPU time used
  std::chrono::microseconds sys_time;

  // Number of esp objects.
  int num_esp;

  // Struct to store esp statistics and service name.
  struct EspData {
    char service_name[kMaxServiceNameSize];
    ApiManagerStatistics statistics;
  };
  EspData esp_stats[kMaxEspNum];

} ngx_esp_process_stats_t;

// Adds shared memory for process stats
ngx_int_t ngx_esp_add_stats_shared_memory(ngx_conf_t *conf);

// Initialize process stats
ngx_int_t ngx_esp_init_process_stats(ngx_cycle_t *cycle);

// Endpoints status content handler
ngx_int_t ngx_esp_status_handler(ngx_http_request_t *r);

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // NGINX_NGX_ESP_STATUS_H_
