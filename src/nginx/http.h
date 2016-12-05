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
#ifndef NGINX_NGX_ESP_HTTP_H_
#define NGINX_NGX_ESP_HTTP_H_

#include <memory>
#include <sstream>

#include "contrib/endpoints/include/api_manager/http_request.h"
#include "contrib/endpoints/include/api_manager/utils/status.h"

extern "C" {
#include "third_party/nginx/src/http/ngx_http.h"
}

namespace google {
namespace api_manager {
namespace nginx {

// ngx_esp_http_connection contains all of the connection data necessary
// to issue an HTTP request using NGINX's upstream module.
//
// Even though all of the data structures are allocated from the NGINX
// connection pool, we additionally contain them all in this single
// data structure so that they can be allocated in one go. The pool size
// is calculated exactly to fit any NGINX pool structures, this struct,
// and pool cleanup callback (which will call the destructor).
struct ngx_esp_http_connection {
  // A 'fake' client NGINX connection object.
  //
  // Because NGINX is primarily a proxy, it usually has two connections:
  // one from the client (downstream), the other to the server (upstream).
  // To use NGINX as an HTTP client, we create a fake client connection
  // and a request (below), and use NGINX upstream module to create the
  // upstream connection to do the actual communication.
  ngx_connection_t connection;

  // Event structures referenced by the connection.
  ngx_event_t read_event;
  ngx_event_t write_event;

  // Local socket address for the fake connection.
  sockaddr_in local_sockaddr;

  // Log and its context.
  // The log context connection will be the fake connection, and the
  // log context request will be the request (below, field `request`).
  ngx_log_t log;
  ngx_http_log_ctx_t log_ctx;

  // Upstream connection configuration, timeouts etc.
  ngx_http_upstream_conf_t upstream_conf;

  // Request information.

  // The nginx_http_request_t used to handle the HTTP request.
  ngx_http_request_t *request;

  // Target URL path and host. Host will be used to send 'Host' header.
  ngx_str_t url_path;
  ngx_str_t host_header;

  // A unique pointer to the HTTP request object created by the caller
  // (contains headers, body, HTTP verb, URL, timeout, and completion
  // continuation).
  std::unique_ptr<HTTPRequest> esp_request;

  // Response information.

  // Parsed HTTP response status.
  ngx_http_status_t response_status;

  // Stream in which we accumulate response body as it is streamed to us
  // by the NGINX upstream module.
  std::ostringstream response_body;

  // Response headers captured in a map.
  std::map<std::string, std::string> response_headers;

  // Wake up information.

  // An event pre-allocated for the tear-down of the request.
  //
  // Because NGINX calls the event handlers in the context of a connection
  // or a request, the memory pools allocated to handle the HTTP request
  // can be accessed when the event handler has returned. Therefore, once
  // the request is complete, we schedule a single top-level event and
  // destroy the memory pools there.
  //
  // see wakeup_event_handler for details.
  ngx_event_t wakeup_event;
};

// Sends an HTTP request and, upon completion (or error), calls the
// continuation.
//
// Continuation stored on the request will be called with the response (or with
// an error, for example on connection failure, timeout etc.)
void ngx_esp_send_http_request(std::unique_ptr<HTTPRequest> request);

}  // namespace nginx
}  // namespace api_manager
}  // namespace google

#endif  // NGINX_NGX_ESP_HTTP_H_
