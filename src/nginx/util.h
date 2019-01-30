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
#ifndef NGINX_NGX_ESP_UTIL_H_
#define NGINX_NGX_ESP_UTIL_H_

#include <cstdlib>
#include <string>
#include <utility>

extern "C" {
#include "src/http/ngx_http.h"
}

#include "google/protobuf/stubs/stringpiece.h"

namespace google {
namespace api_manager {
namespace nginx {

// Copy string from std::string to ngx_str_t.
ngx_int_t ngx_str_copy_from_std(ngx_pool_t *pool, const std::string &src,
                                ngx_str_t *dst);

// convert ngx_str_t to StringPiece.
::google::protobuf::StringPiece ngx_str_to_stringpiece(const ngx_str_t &src);

// Convert a ngx_str_t to std::string.
std::string ngx_str_to_std(const ngx_str_t &src);

// Create a ngx_str_t that points to the underlying contents of src.
// This does *not* copy the src string; any mutations to src will
// corrupt the ngx_str_t.  On the other hand, it can't fail, and works
// fine as long as src remains unchanged as long as the ngx_str_t
// exists.
ngx_str_t ngx_std_to_str_unsafe(const std::string &src);

// Create a heap-allocated copy of a string.
ngx_str_t ngx_esp_copy_string(const char *src);

// Compare two ngx_str_t strings
inline bool ngx_string_equal(const ngx_str_t &str1, const ngx_str_t &str2) {
  return str1.len == str2.len && !ngx_strncmp(str1.data, str2.data, str1.len);
}

// Log a message with a specific log level.
void ngx_esp_log(ngx_log_t *log, ngx_uint_t level, ngx_str_t msg);

// Extract HTTP response status code.
ngx_uint_t ngx_http_get_response_status(ngx_http_request_t *r);

// Search HTTP request headers.
ngx_table_elt_t *ngx_esp_find_headers_in(ngx_http_request_t *r, u_char *name,
                                         size_t len);

// Search HTTP response headers.
ngx_table_elt_t *ngx_esp_find_headers_out(ngx_http_request_t *r, u_char *name,
                                          size_t len);

// An InputIterator for nginx headers.
class ngx_esp_header_iterator {
 public:
  typedef std::input_iterator_tag iterator_category;
  typedef ngx_table_elt_t value_type;
  typedef value_type *pointer;
  typedef value_type &reference;

  ngx_esp_header_iterator();
  explicit ngx_esp_header_iterator(ngx_list_t *headers);

  bool operator==(const ngx_esp_header_iterator &it) const;
  bool operator!=(const ngx_esp_header_iterator &it) const;
  reference operator*() const;
  pointer operator->() const;
  ngx_esp_header_iterator &operator++();    // pre-increment
  ngx_esp_header_iterator operator++(int);  // post-increment

 private:
  ngx_list_part_t *part_;
  ngx_table_elt_t *header_;
  ngx_uint_t i_;
};

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

// These definitions need to be outside of the namespace -- see
// http://en.cppreference.com/w/cpp/language/adl for details.
::google::api_manager::nginx::ngx_esp_header_iterator begin(
    ngx_http_headers_in_t &in);
::google::api_manager::nginx::ngx_esp_header_iterator end(
    ngx_http_headers_in_t &in);
::google::api_manager::nginx::ngx_esp_header_iterator begin(
    ngx_http_headers_out_t &out);
::google::api_manager::nginx::ngx_esp_header_iterator end(
    ngx_http_headers_out_t &out);

#endif  // NGINX_NGX_ESP_UTIL_H_
