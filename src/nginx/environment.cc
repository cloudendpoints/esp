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
#include "src/nginx/environment.h"

#include "src/nginx/http.h"
#include "src/nginx/util.h"

#include <stdexcept>

namespace google {
namespace api_manager {
namespace nginx {

void NgxEspEnv::Log(LogLevel level, const char *message) {
  ngx_uint_t ngx_level;
  switch (level) {
    case DEBUG:
      ngx_level = NGX_LOG_DEBUG;
      break;
    case INFO:
      ngx_level = NGX_LOG_INFO;
      break;
    case WARNING:
      ngx_level = NGX_LOG_WARN;
      break;
    case ERROR:
    default:
      ngx_level = NGX_LOG_ERR;
      break;
  }

  ngx_str_t msg = {strlen(message),
                   reinterpret_cast<u_char *>(const_cast<char *>(message))};
  ngx_esp_log(log_, ngx_level, msg);
}

NgxEspTimer::NgxEspTimer(std::chrono::milliseconds interval,
                         std::function<void()> callback, ngx_log_t *log)
    : stopped_(false), interval_(interval), callback_(callback), log_(log) {
  AddTimer();
}

NgxEspTimer::~NgxEspTimer() {
  Stop();
  // In nginx's single-threaded environment, we're already
  // synchronized with timer invocations, so we do not synchronize
  // here.
}

void NgxEspTimer::Stop() {
  if (ev_.timer_set) {
    ngx_del_timer(&ev_);
  }
  stopped_ = true;
}

void NgxEspTimer::AddTimer() {
  ngx_memzero(&ev_, sizeof(ev_));
  ev_.data = reinterpret_cast<void *>(this);
  ev_.handler = &NgxEspTimer::OnExpiration;
  ev_.log = log_;
  ev_.cancelable = 1;
  ngx_add_timer(&ev_, interval_.count());
}

void NgxEspTimer::OnExpiration(ngx_event_t *ev) {
  if (ev->timer_set || !ev->timedout) {
    return;
  }
  NgxEspTimer *t = reinterpret_cast<NgxEspTimer *>(ev->data);
  t->callback_();
  if (!t->stopped_) {
    t->AddTimer();
  }
}

std::unique_ptr<PeriodicTimer> NgxEspEnv::StartPeriodicTimer(
    std::chrono::milliseconds interval, std::function<void()> continuation) {
  return std::unique_ptr<PeriodicTimer>(
      new NgxEspTimer(interval, continuation, log_));
}

void NgxEspEnv::RunHTTPRequest(std::unique_ptr<HTTPRequest> request) {
  ngx_esp_send_http_request(std::move(request));
}

void NgxEspEnv::RunGRPCRequest(std::unique_ptr<GRPCRequest> request) {}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
