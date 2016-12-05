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
#ifndef NGINX_NGX_ESP_ENV_H_
#define NGINX_NGX_ESP_ENV_H_

#include "contrib/endpoints/include/api_manager/api_manager.h"

extern "C" {
#include "third_party/nginx/src/core/ngx_core.h"
#include "third_party/nginx/src/http/ngx_http.h"
}

#include "src/nginx/grpc_queue.h"

namespace google {
namespace api_manager {
namespace nginx {

// The nginx implementation of ApiManagerEnvInterface.
class NgxEspEnv : public ApiManagerEnvInterface {
 public:
  NgxEspEnv(ngx_log_t *log) : log_(log) {}

  virtual ~NgxEspEnv() {}

  virtual void Log(LogLevel level, const char *message);

  virtual std::unique_ptr<PeriodicTimer> StartPeriodicTimer(
      std::chrono::milliseconds interval, std::function<void()> continuation);

  virtual void RunHTTPRequest(std::unique_ptr<HTTPRequest> request);

 private:
  ngx_log_t *log_;
};

// The nginx implementation of PeriodicTimer.
class NgxEspTimer : public PeriodicTimer {
 public:
  NgxEspTimer(std::chrono::milliseconds interval,
              std::function<void()> callback, ngx_log_t *log);

  virtual ~NgxEspTimer();

  virtual void Stop();

 private:
  void AddTimer();

  static void OnExpiration(ngx_event_t *ev);

  ngx_event_t ev_;
  bool stopped_;
  std::chrono::milliseconds interval_;
  std::function<void()> callback_;
  ngx_log_t *log_;
};

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // NGINX_NGX_ESP_ENV_H_
