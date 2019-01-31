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
#include "src/nginx/util.h"

#include <cstdio>

using ::google::protobuf::StringPiece;

namespace google {
namespace api_manager {
namespace nginx {

ngx_int_t ngx_str_copy_from_std(ngx_pool_t *pool, const std::string &src,
                                ngx_str_t *dst) {
  u_char *data = reinterpret_cast<u_char *>(ngx_pcalloc(pool, src.size() + 1));
  if (!data) {
    return NGX_ERROR;
  }
  ngx_memcpy(data, src.data(), src.size());
  dst->data = data;
  dst->len = src.size();
  return NGX_OK;
}

::google::protobuf::StringPiece ngx_str_to_stringpiece(const ngx_str_t &src) {
  return ::google::protobuf::StringPiece(
      reinterpret_cast<const char *>(src.data), src.len);
}

std::string ngx_str_to_std(const ngx_str_t &src) {
  return (src.data == nullptr || src.len <= 0)
             ? std::string()
             : std::string(reinterpret_cast<const char *>(src.data), src.len);
}

ngx_str_t ngx_std_to_str_unsafe(const std::string &src) {
  ngx_str_t result;
  result.data = reinterpret_cast<u_char *>(const_cast<char *>(src.c_str()));
  result.len = src.length();
  return result;
}

ngx_str_t ngx_esp_copy_string(const char *src) {
  char *copy = strdup(src);
  if (copy) {
    return {strlen(copy), reinterpret_cast<u_char *>(copy)};
  } else {
    return ngx_null_string;
  }
}

namespace {

const char *log_levels[9] = {
    "",        // NGX_LOG_STDERR
    "emerg",   // NGX_LOG_EMERG
    "alert",   // NGX_LOG_ALERT
    "crit",    // NGX_LOG_CRIT
    "error",   // NGX_LOG_ERR
    "warn",    // NGX_LOG_WARN
    "notice",  // NGX_LOG_NOTICE
    "info",    // NGX_LOG_INFO
    "debug",   // NGX_LOG_DEBUG
};
const ngx_uint_t log_levels_count = sizeof(log_levels) / sizeof(log_levels[0]);

}  // namespace

#define HEADER_SIZE 1024
#define TRAILER_SIZE 512

void ngx_esp_log(ngx_log_t *log, ngx_uint_t level, ngx_str_t msg) {
  if (!log || log->log_level < level) return;

  if (level >= log_levels_count) {
    level = log_levels_count - 1;
  }

  u_char header[HEADER_SIZE];
  u_char trailer[TRAILER_SIZE];
  u_char *h = header, *const last = header + HEADER_SIZE;
  u_char *t = trailer;

  h = ngx_cpymem(h, ngx_cached_err_log_time.data, ngx_cached_err_log_time.len);
  h = ngx_slprintf(h, last, "[%s]", log_levels[level]);
  h = ngx_slprintf(h, last, "%P#" NGX_TID_T_FMT ": ", ngx_log_pid, ngx_log_tid);
  if (log->connection) {
    h = ngx_slprintf(h, last, "*%uA ", log->connection);
  }
  if (level != NGX_LOG_DEBUG && log->handler) {
    t = log->handler(log, t, TRAILER_SIZE - 1);  // space for ngx_linefeed
  }
  ngx_linefeed(t);

  bool debug_connection = !!(log->log_level & NGX_LOG_DEBUG_CONNECTION);
  bool wrote_stderr = false;

  for (; log; log = log->next) {
    if (log->log_level < level && !debug_connection) {
      break;
    }

    if (log->writer) {
      log->writer(log, level, header, h - header);
      log->writer(log, level, msg.data, msg.len);
      log->writer(log, level, trailer, t - trailer);
    } else {
      ngx_fd_t fd = log->file->fd;
      ngx_write_fd(fd, header, h - header);
      ngx_write_fd(fd, msg.data, msg.len);
      ngx_write_fd(fd, trailer, t - trailer);

      if (fd == ngx_stderr) {
        wrote_stderr = true;
      }
    }
  }

  if (ngx_use_stderr && level <= NGX_LOG_WARN && !wrote_stderr) {
    h = ngx_slprintf(header, last, "nginx: [%s] ", log_levels[level]);
    ngx_write_fd(ngx_stderr, header, h - header);
    ngx_write_fd(ngx_stderr, msg.data, msg.len);
    ngx_write_fd(ngx_stderr, trailer, t - trailer);
  }
}

// Extract HTTP response status code.
ngx_uint_t ngx_http_get_response_status(ngx_http_request_t *r) {
  if (r->err_status) {
    return r->err_status;
  } else if (r->headers_out.status) {
    return r->headers_out.status;
  } else if (r->http_version == NGX_HTTP_VERSION_9) {
    return 9;
  } else {
    return 0;
  }
}

ngx_table_elt_t *ngx_esp_find_headers_in(ngx_http_request_t *r, u_char *name,
                                         size_t len) {
  for (auto &h : r->headers_in) {
    if (len == h.key.len && ngx_strcasecmp(name, h.key.data) == 0) {
      return &h;
    }
  }
  return nullptr;
}

ngx_table_elt_t *ngx_esp_find_headers_out(ngx_http_request_t *r, u_char *name,
                                          size_t len) {
  for (auto &h : r->headers_out) {
    if (len == h.key.len && ngx_strcasecmp(name, h.key.data) == 0) {
      return &h;
    }
  }
  return nullptr;
}

ngx_esp_header_iterator::ngx_esp_header_iterator()
    : part_(nullptr), header_(nullptr), i_(0) {}

ngx_esp_header_iterator::ngx_esp_header_iterator(ngx_list_t *headers)
    : part_(&headers->part), header_(nullptr), i_(0) {
  if (part_) {
    i_ = -1;  // Give operator++ something to increment.
    operator++();
  }
}

bool ngx_esp_header_iterator::operator==(
    const ngx_esp_header_iterator &it) const {
  return (part_ == it.part_ && header_ == it.header_ && i_ == it.i_);
}

bool ngx_esp_header_iterator::operator!=(
    const ngx_esp_header_iterator &it) const {
  return !(*this == it);
}

ngx_table_elt_t &ngx_esp_header_iterator::operator*() const {
  return header_[i_];
}

ngx_table_elt_t *ngx_esp_header_iterator::operator->() const {
  return header_ + i_;
}

ngx_esp_header_iterator &ngx_esp_header_iterator::operator++() {
  if (!part_) {
    return *this;
  }
  i_++;
  while (part_ && i_ >= part_->nelts) {
    part_ = part_->next;
    i_ = 0;
  }
  header_ = part_ ? reinterpret_cast<ngx_table_elt_t *>(part_->elts) : nullptr;
  return *this;
}

ngx_esp_header_iterator ngx_esp_header_iterator::operator++(int) {
  ngx_esp_header_iterator it(*this);
  operator++();
  return it;
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

::google::api_manager::nginx::ngx_esp_header_iterator begin(
    ngx_http_headers_in_t &in) {
  return ::google::api_manager::nginx::ngx_esp_header_iterator(&in.headers);
}

::google::api_manager::nginx::ngx_esp_header_iterator end(
    ngx_http_headers_in_t &in) {
  return ::google::api_manager::nginx::ngx_esp_header_iterator();
}

::google::api_manager::nginx::ngx_esp_header_iterator begin(
    ngx_http_headers_out_t &out) {
  return ::google::api_manager::nginx::ngx_esp_header_iterator(&out.headers);
}

::google::api_manager::nginx::ngx_esp_header_iterator end(
    ngx_http_headers_out_t &out) {
  return ::google::api_manager::nginx::ngx_esp_header_iterator();
}
