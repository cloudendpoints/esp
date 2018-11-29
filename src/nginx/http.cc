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
#include "src/nginx/http.h"

#include <core/ngx_string.h>
#include <memory>

#include "include/api_manager/http_request.h"
#include "src/nginx/alloc.h"
#include "src/nginx/module.h"
#include "src/nginx/util.h"

extern "C" {
#include "src/core/ngx_core.h"
#include "src/http/ngx_http.h"
}

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {
namespace nginx {

namespace {

// The pool size used for the outgoing HTTP request.
const size_t kRequestPoolSize = 4096;

// Default HTTP timeout, in milliseconds.
const int kDefaultTimeoutMilliseconds = 60000;

// http:// and https:// prefixes used to parse target URL of the HTTP
// request and identify the protocol.
ngx_str_t http = ngx_string("http://");
#if NGX_HTTP_SSL
ngx_str_t https = ngx_string("https://");
#endif

// Declarations of response handlers.
ngx_int_t ngx_esp_upstream_process_status_line(ngx_http_request_t *r);
ngx_int_t ngx_esp_upstream_process_header(ngx_http_request_t *r);

// Alternatively, we could inherit both ngx_esp_http_connection and
// ngx_esp_request_ctx_s from a common base and store the base pointer
// in the per-request module context.
inline ngx_esp_http_connection *get_esp_connection(ngx_http_request_t *r) {
  if (r != nullptr) {
    ngx_esp_request_ctx_t *dummy = reinterpret_cast<ngx_esp_request_ctx_t *>(
        ngx_http_get_module_ctx(r, ngx_esp_module));
    ngx_esp_http_connection *ehc = dummy->http_subrequest;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "get_esp_connection(%p) -> %p", r, ehc);

    return ehc;
  } else {
    return nullptr;
  }
}

// Parses the request URL, identifies the URL scheme and default port,
//
Status ngx_esp_upstream_set_url(ngx_pool_t *pool, ngx_http_upstream_t *upstream,
                                ngx_str_t url, ngx_str_t *host_header,
                                ngx_str_t *url_path) {
  ngx_url_t parsed_url;
  ngx_memzero(&parsed_url, sizeof(parsed_url));

  // Recognize the URL scheme.
  // only if NGINX has been compiled with NGX_HTTP_SSL support, try to recognize
  // a https:// scheme because the ssl related fields are not defined otherwise.
  if (url.len > http.len &&
      ngx_strncasecmp(url.data, http.data, http.len) == 0) {
    upstream->schema = http;

    parsed_url.url.data = url.data + http.len;
    parsed_url.url.len = url.len - http.len;
    parsed_url.default_port = 80;

#if NGX_HTTP_SSL
  } else if (url.len > https.len &&
             ngx_strncasecmp(url.data, https.data, https.len) == 0) {
    upstream->schema = https;
    upstream->ssl = 1;

    parsed_url.url.data = url.data + https.len;
    parsed_url.url.len = url.len - https.len;
    parsed_url.default_port = 443;
#endif
  } else {
    return Status(NGX_ERROR, "Invalid URL scheme");
  }

  // Parse out the URI part of the URL.
  parsed_url.uri_part = 1;
  // Do not try to resolve the host name as part of URL parsing.
  parsed_url.no_resolve = 1;

  if (ngx_parse_url(pool, &parsed_url) != NGX_OK) {
    return Status(NGX_ERROR, "Cannot parse URL");
  }

  // Detect situation where input URL was of the form:
  //     http://domain.com?query_parameters
  // (without a forward slash preceding a question mark), and insert
  // a leading slash to form a valid path: /?query_parameters.
  if (parsed_url.uri.len > 0 && parsed_url.uri.data[0] == '?') {
    u_char *p =
        reinterpret_cast<u_char *>(ngx_pnalloc(pool, parsed_url.uri.len + 1));
    if (p == nullptr) {
      return Status(NGX_ERROR, "Out of memory");
    }

    *p = '/';
    ngx_memcpy(p + 1, parsed_url.uri.data, parsed_url.uri.len);
    parsed_url.uri.len++;
    parsed_url.uri.data = p;
  }

  // Populate the upstream config structure with the parsed URL.

  upstream->resolved = reinterpret_cast<ngx_http_upstream_resolved_t *>(
      ngx_pcalloc(pool, sizeof(ngx_http_upstream_resolved_t)));
  if (upstream->resolved == nullptr) {
    return Status(NGX_ERROR, "Out of memory");
  }

  // This condition is true if the URL did not use domain name, but rather
  // an IP address directly: http://74.125.25.239/...
  // NGINX, in this case, only returns the parsed IP address so there is
  // only one address to return. See ngx_parse_url and ngx_inet_addr for
  // details.
  if (parsed_url.addrs != nullptr && parsed_url.addrs[0].sockaddr) {
    upstream->resolved->sockaddr = parsed_url.addrs[0].sockaddr;
    upstream->resolved->socklen = parsed_url.addrs[0].socklen;
    upstream->resolved->naddrs = 1;
    upstream->resolved->host = parsed_url.addrs[0].name;
  } else {
    upstream->resolved->host = parsed_url.host;
  }

  upstream->resolved->no_port = parsed_url.no_port;
  upstream->resolved->port =
      parsed_url.no_port ? parsed_url.default_port : parsed_url.port;

  // Return Host header and a URL path.
  if (parsed_url.family != AF_UNIX) {
    *host_header = parsed_url.host;
    // If the URL contains a port, include it in the host header.
    if (!(parsed_url.no_port || parsed_url.port == parsed_url.default_port)) {
      // Extend the Host header to include ':<port>'
      host_header->len += 1 + parsed_url.port_text.len;
    }
  } else {
    ngx_str_set(host_header, "localhost");
  }
  *url_path = parsed_url.uri;

  return Status::OK;
}

// Utilities to render HTTP request inside an NGINX buffer.
//
// Overloads accepting const std::string&, ngx_str_t and a string literal
// are available.

inline void append(ngx_buf_t *buf, const std::string &value) {
  buf->last = ngx_cpymem(buf->last, value.c_str(), value.size());
}

inline void append(ngx_buf_t *buf, ngx_str_t value) {
  buf->last = ngx_cpymem(buf->last, value.data, value.len);
}

// The overload which accepts a string literal.
// It is templetized on the char array size and returns size of the array - 1
// (for trailing \0).
template <size_t n>
inline void append(ngx_buf_t *buf, const char (&value)[n]) {
  buf->last = ngx_cpymem(buf->last, value, sizeof(value) - 1);
}

//
// HTTP Request upstream handlers.
//
// NGINX upstream module is responsible for establishing and maintaining the
// peer connection, SSH, reading response data. It does not actually construct
// the request being sent to the upstream, or parse the response.
//
// The caller provides the upstream module with several callbacks which create
// the request buffers, reinitialize state for retries, and parse the response.
// These callbacks are implemented below.
//

// Create request.
// When NGINX is ready to send the data to the upstream, it calls this handler
// to create the request buffer.
// It will compute needed buffer size, allocate the buffer, and create an HTTP
// request within it, passing it back to NGINX for network communication.
ngx_int_t ngx_esp_upstream_create_request(ngx_http_request_t *r) {
  ngx_esp_http_connection *http_connection = get_esp_connection(r);
  ngx_log_debug2(
      NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
      "esp: ngx_esp_upstream_create_request (r=%p, http_connection=%p)", r,
      http_connection);
  if (http_connection == nullptr) {
    return NGX_ERROR;
  }

  // Accumulate buffer size.
  size_t buffer_size = 0;

  HTTPRequest *http_request = http_connection->esp_request.get();

  ngx_log_debug4(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "esp: creating request (r=%p), %s %V%V", r,
                 http_request->method().c_str(), &http_connection->host_header,
                 &http_connection->url_path);

  // <method> followed by a space.
  buffer_size += http_request->method().size() + sizeof(" ") - 1;
  // <URL path> followed by 'HTTP/1.0' and a newline.
  buffer_size += http_connection->url_path.len + sizeof(" HTTP/1.0" CRLF) - 1;
  // 'Host:' header, followed by a newline.
  buffer_size += sizeof("Host: ") - 1;
  buffer_size += http_connection->host_header.len;
  buffer_size += sizeof(CRLF) - 1;
  // Right now all our subrequests close connection (both old and new
  // implementation).
  // TODO: investigate NGINX ability for connection reuse. This may especially
  // benefit service control connections.
  buffer_size += sizeof("Connection: close" CRLF) - 1;

  // Add sizes of all headers and their values.
  for (const auto &header : http_request->request_headers()) {
    const std::string &name = header.first;
    const std::string &value = header.second;

    buffer_size += name.size();
    buffer_size += sizeof(": ") - 1;
    buffer_size += value.size();
    buffer_size += sizeof(CRLF) - 1;
  }

  // If the request accepts a body, add Content-Length header and the body size.
  bool request_accepts_body = http_request->method() == "POST" ||
                              http_request->method() == "PUT" ||
                              http_request->method() == "PATCH";
  if (request_accepts_body) {
    // Add space for Content-Length header and its value.
    buffer_size +=
        sizeof("Content-Length: ") - 1 + NGX_OFF_T_LEN + sizeof(CRLF) - 1;

    // Add the body size.
    buffer_size += http_request->body().size();
  }

  buffer_size += sizeof(CRLF) - 1;  // Newline following the HTTP headers.

  // Create a temporary buffer to render the request.
  ngx_buf_t *buf = ngx_create_temp_buf(r->pool, buffer_size);
  if (buf == nullptr) {
    return NGX_ERROR;
  }

  // Append an HTTP request line.
  append(buf, http_request->method());
  append(buf, " ");
  append(buf, http_connection->url_path);
  // Must be HTTP/1.0 since this module doesn't support HTTP/1.1 features;
  // such as trunked encoding.
  append(buf, " HTTP/1.0" CRLF);

  // Append the Host and Connection headers.
  append(buf, "Host: ");
  append(buf, http_connection->host_header);
  append(buf, CRLF);
  append(buf, "Connection: close" CRLF);

  // Append the headers provided by the caller.
  for (const auto &header : http_request->request_headers()) {
    const std::string &name = header.first;
    const std::string &value = header.second;

    append(buf, name);
    append(buf, ": ");
    append(buf, value);
    append(buf, CRLF);
  }

  // Append content-length header if needed.
  if (request_accepts_body) {
    append(buf, "Content-Length: ");
    buf->last =
        ngx_sprintf(buf->last, "%O", (off_t)http_request->body().size());
    append(buf, CRLF);
  }

  // End request headers, insert newline before the body.
  append(buf, CRLF);

  if (request_accepts_body && http_request->body().size() > 0) {
    append(buf, http_request->body());
  }

  // Allocate a buffer chain for NGINX.
  ngx_chain_t *chain = ngx_alloc_chain_link(r->pool);
  if (chain == nullptr) {
    return NGX_ERROR;
  }

  chain->next = nullptr;
  chain->buf = buf;

  // We are only sending one buffer.
  buf->last_buf = 1;

  // Attach the buffer to the request.
  r->upstream->request_bufs = chain;
  r->subrequest_in_memory = 1;

  return NGX_OK;
}

// A handler NGINX calls when it needs to reinitialize response processing
// state machine.
ngx_int_t ngx_esp_upstream_reinit_request(ngx_http_request_t *r) {
  ngx_esp_http_connection *http_connection = get_esp_connection(r);

  ngx_log_debug2(
      NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
      "esp: ngx_esp_upstream_reinit_request (r=%p, http_connection=%p)", r,
      http_connection);

  if (http_connection == nullptr) {
    return NGX_ERROR;
  }

  // We only reset state to start parsing status line again.
  r->upstream->process_header = ngx_esp_upstream_process_status_line;
  return NGX_OK;
}

// A handler called to parse response status line.
//
// NGINX only has one callback for parsing 'header' in general so once we have
// successfully parsed the HTTP status line, we update the handler to point
// at the header parsing function. This is a technique used in other
// implementations of upstream modules.
ngx_int_t ngx_esp_upstream_process_status_line(ngx_http_request_t *r) {
  ngx_esp_http_connection *http_connection = get_esp_connection(r);

  ngx_log_debug2(
      NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
      "esp: ngx_esp_upstream_process_status_line (r=%p, http_connection=%p)", r,
      http_connection);

  if (http_connection == nullptr) {
    return NGX_ERROR;
  }

#if (NGX_DEBUG)
  // Get the buffer with the response arriving from the upstream server.
  ngx_buf_t *buf = &r->upstream->buffer;

  // str is only used in debug mode
  ngx_str_t str = {(size_t)(buf->last - buf->pos), buf->pos};
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "Received (partial) response:\n%V\n\n", &str);
#endif

  // Parse the status line by calling NGINX helper.
  ngx_http_status_t status;
  ngx_memzero(&status, sizeof(status));

  ngx_int_t rc = ngx_http_parse_status_line(r, &r->upstream->buffer, &status);
  if (rc == NGX_AGAIN) {
    return rc;  // We don't have the whole status line yet.
  } else if (rc == NGX_ERROR) {
    r->upstream->headers_in.connection_close = 1;
    return NGX_OK;
  }

  // Store the parsed response status for later (it will be passed to the
  // continuation as a status).
  http_connection->response_status = status;

  // Advance the state machine to parse individual headers next.
  r->upstream->process_header = ngx_esp_upstream_process_header;
  return ngx_esp_upstream_process_header(r);
}

// A handler called by NGINX to parse response headers.
ngx_int_t ngx_esp_upstream_process_header(ngx_http_request_t *r) {
  ngx_esp_http_connection *http_connection = get_esp_connection(r);

  ngx_log_debug2(
      NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
      "esp: ngx_esp_upstream_process_header (r=%p, http_connection=%p)", r,
      http_connection);

  if (http_connection == nullptr) {
    return NGX_ERROR;
  }

  for (;;) {
    // Parse an individual header line by calling an NGINX helper.
    ngx_int_t rc = ngx_http_parse_header_line(r, &r->upstream->buffer, 1);

    if (rc == NGX_OK) {
      ngx_str_t name = {(size_t)(r->header_name_end - r->header_name_start),
                        r->header_name_start};
      ngx_strlow(name.data, name.data, name.len);
      ngx_str_t value = {(size_t)(r->header_end - r->header_start),
                         r->header_start};
      // Store headers if it is required.
      if (http_connection->esp_request->requires_response_headers()) {
        http_connection->response_headers.emplace(ngx_str_to_std(name),
                                                  ngx_str_to_std(value));
      }
      ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                     "esp: header: %V: %V", &name, &value);

      // Check if the header was "Content-Length".
      static ngx_str_t content_length = ngx_string("Content-Length");
      if (name.len == content_length.len &&
          ngx_strncasecmp(name.data, content_length.data, content_length.len) ==
              0) {
        // Store the content length on the incoming headers object.
        r->upstream->headers_in.content_length_n =
            ngx_atoof(value.data, value.len);
      }
    } else if (rc == NGX_HTTP_PARSE_HEADER_DONE) {
      return NGX_OK;
    } else if (rc == NGX_AGAIN) {
      return NGX_AGAIN;
    } else {
      return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }
  }
}

// An abort handler -- apparently this is never called by NGINX so we only log.
void ngx_esp_upstream_abort_request(ngx_http_request_t *r) {
  ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                "endpoints request aborted");
}

//
// Wakeup handler.
//
// Once the request completes, the callbacks from NGINX are happening in the
// context of our data structures and NGINX may be accessing them once we return
// from ngx_esp_upstream_finalize_request handler. Therefore, we cannot destroy
// NGINX buffers in the finalize handler. Instead, we schedule a callback for
// the next iteration of NGINX main event loop.
//
// This is the function that we register NGINX to call.
// It will destroy the pools we created to handle the request and, if parent
// request was provided by the caller, wake it up.
//
void wakeup_event_handler(ngx_event_t *ev) {
  ngx_esp_http_connection *http_connection =
      reinterpret_cast<ngx_esp_http_connection *>(ev->data);

  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                 "esp: wakeup_event_handler %p, data==%p", ev, ev->data);
  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                 "wakeup_event_handler called: %V%V",
                 &http_connection->host_header, &http_connection->url_path);

  // Move all pointers to the local variables because once we destroy the
  // pools we can no longer access the memory.

  // Request and a connection pools.
  ngx_pool_t *rp = http_connection->request->pool;
  ngx_pool_t *cp = http_connection->connection.pool;

  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                 "esp: destroying pools c=%p, r=%p", cp, rp);

  // If the connection is reset by peer, somehow rp or cp is 0.
  // Not sure if any of pools have been freed. But trying to free
  // a nullptr pool definitely will cause crash.
  // Here, choose the least of evil, memory leak over crash.
  if (rp == nullptr || cp == nullptr) {
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                   "esp memory pools may not be freed: pools c=%p, r=%p", cp,
                   rp);
    return;
  }

  // Destroy the pools.
  ngx_destroy_pool(rp);
  ngx_destroy_pool(cp);
}

// A finalize handler. Called by NGINX when request is complete (success)
// or on error, for example connection error, timeout, etc.
void ngx_esp_upstream_finalize_request(ngx_http_request_t *r, ngx_int_t rc) {
  ngx_esp_http_connection *http_connection = get_esp_connection(r);

  ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "esp: finalizing request r=%p, rc=%d, http_connection=%p", r,
                 rc, http_connection);

  if (http_connection == nullptr) {
    return;
  }

  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "ngx_esp_upstream_finalize_request called: %V%V",
                 &http_connection->host_header, &http_connection->url_path);

  std::string message;
  if (rc == NGX_OK) {
    // If the overall transmission succeeded (rc == NGX_OK), use the HTTP
    // status code as a parameter for the continuation.
    rc = http_connection->response_status.code;
    // Fills in error message if status code is not 200.
    if (rc != 200) {
      message = std::string("server response status code: ");
      if (http_connection->response_status.start) {
        message += std::string(reinterpret_cast<const char *>(
                                   http_connection->response_status.start),
                               http_connection->response_status.count);
      }
    }
  } else {
    rc = NGX_ERROR;
    message = "Failed to connect to server.";
  }

  // Call the continuation.
  if (http_connection->esp_request) {
    // Swap the initial HTTP request out to make sure we don't
    // call the continuation multiple times.
    std::unique_ptr<HTTPRequest> request;
    request.swap(http_connection->esp_request);

    // Retry if an error and retry budget left
    if (rc == NGX_ERROR && request->max_retries() > 0) {
      // increase timeout
      request->set_max_retries(request->max_retries() - 1);
      request->set_timeout_ms(request->timeout_ms() *
                              request->timeout_backoff_factor());

      ngx_log_debug1(NGX_LOG_DEBUG_HTTP, &http_connection->log, 0,
                     "Retrying connection, max retries left %d",
                     request->max_retries());

      // Swap the initial HTTP request out to the next iteration
      ngx_esp_send_http_request(std::move(request));
    } else {
      // Extract accumulated response body to a string.
      std::string body = http_connection->response_body.str();

      Status status(rc, message);

      ngx_log_debug1(NGX_LOG_DEBUG_HTTP, &http_connection->log, 0,
                     "Calling a continuation with status: %s",
                     status.ToString().c_str());
      ngx_log_debug1(NGX_LOG_DEBUG_HTTP, &http_connection->log, 0,
                     "Calling a continuation with a body size: %d",
                     body.size());

      // Call the request continuation.
      request->OnComplete(status, std::move(http_connection->response_headers),
                          std::move(body));
    }
  } else {
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, &http_connection->log, 0,
                   "continuation not available. skipping call");
  }

  // Set up the wake-up event.
  // We schedule it for the next iteration through the event loop
  // so that we can destroy the pools which can still be accessed
  // on the return path out of this call stack.
  http_connection->wakeup_event.data = http_connection;
  http_connection->wakeup_event.write = 1;
  http_connection->wakeup_event.handler = wakeup_event_handler;
  http_connection->wakeup_event.log = &http_connection->log;

  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, &http_connection->log, 0,
                 "esp: Posting wakeup event %p, data==%p",
                 &http_connection->wakeup_event, http_connection);

  // Schedule the wakeup call with NGINX.
  ngx_post_event(&http_connection->wakeup_event, &ngx_posted_events);
}

// An input filter initialization handler.
// NGINX calls it when the response body starts arriving and the caller can
// initialize any state (for example, allocate buffers).
// We accumulate the body in a stringstream so no action is needed here.
//
// The data pointer is provided to NGINX via input_filter_ctx. Below, we
// set it to the request object.
ngx_int_t ngx_esp_upstream_input_filter_init(void *data) {
  ngx_http_request_t *r = reinterpret_cast<ngx_http_request_t *>(data);
  ngx_esp_http_connection *http_connection = get_esp_connection(r);

  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "esp: ngx_esp_upstream_input_filter_init called (r=%p, "
                 "http_connection=%p)",
                 r, http_connection);

  if (http_connection == nullptr) {
    return NGX_ERROR;
  }

#if (NGX_DEBUG)
  // esp_request is only used in debug mode; the log_debug macro
  // is noop in release
  HTTPRequest *esp_request = http_connection->esp_request.get();
  ngx_log_debug1(
      NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
      "endpoints upstream filter initialized for (%s)",
      esp_request != nullptr ? esp_request->url().c_str() : "<unknown URL>");
#endif

  return NGX_OK;
}

// An upstream input filter handler.
//
// After initialization, NGINX calls this filter handler whenever new data
// is read from the upstream connection.
// We accumulate the data by writing it into a string stream.
ngx_int_t ngx_esp_upstream_input_filter(void *data, ssize_t bytes) {
  ngx_http_request_t *r = reinterpret_cast<ngx_http_request_t *>(data);
  ngx_esp_http_connection *http_connection = get_esp_connection(r);
  ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "esp: ngx_esp_upstream_input_filter called (r=%p, bytes=%d, "
                 "http_connection=%p)",
                 r, bytes, http_connection);
  if (http_connection == nullptr) {
    return NGX_ERROR;
  }

#if (NGX_DEBUG)
  // body only used in debug mode; the log_debug macro is noop in release
  ngx_str_t body = {(size_t)bytes, r->upstream->buffer.last};
  ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "endpoints received %d bytes: %V", (int)bytes, &body);
#endif

  http_connection->response_body.write(
      reinterpret_cast<char *>(r->upstream->buffer.last), bytes);

  return NGX_OK;
}

// An upstream pipe input filter handler.
//
// After initialization, NGINX calls this filter handler whenever new data
// is read from the upstream connection pipe.
// We accumulate the data by writing it into a string stream.
static ngx_int_t ngx_esp_upstream_pipe_input_filter(ngx_event_pipe_t *p,
                                                    ngx_buf_t *buf) {
  ngx_chain_t *cl;
  ngx_http_request_t *r = (ngx_http_request_t *)p->input_ctx;
  if (r == NULL) {
    return NGX_ERROR;
  }

  ngx_esp_http_connection *http_connection = get_esp_connection(r);
  if (http_connection == NULL) {
    return NGX_ERROR;
  }

  if (buf->pos == buf->last) {
    return NGX_OK;
  }

  cl = ngx_chain_get_free_buf(p->pool, &p->free);
  if (cl == NULL) {
    return NGX_ERROR;
  }

  ssize_t bytes = buf->last - buf->pos;
  if (bytes < 0) {
    return NGX_ERROR;
  }
  ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "esp: ngx_esp_upstream_input_filter called (r=%p, bytes=%d, "
                 "http_connection=%p)",
                 r, bytes, http_connection);

  http_connection->response_body.write(reinterpret_cast<char *>(buf->pos),
                                       bytes);
  return NGX_OK;
}

// NGINX calls connections send_chain handler when it wants to send data to the
// connection's client.
// In our case, we don't have any client so we simply discard any data NGINX
// may be trying to send. This happens for example in case NGINX encounters
// errors and tries to send them back to the (in our case nonexistent) client.
ngx_chain_t *esp_http_do_not_send_chain(ngx_connection_t *c, ngx_chain_t *in,
                                        off_t limit) {
  ngx_log_error(NGX_LOG_DEBUG, c->log, 0, "esp_http_do_not_send_chain called.");
  return nullptr;
}

// A utility NGINX pool deleter used with std::unique_ptr.
// It is used below to make the error handling and pool destruction easier.
class ngx_pool_t_deleter {
 public:
  void operator()(ngx_pool_t *p) { ngx_destroy_pool(p); }
};

//
// The creation of the request data structures.
//
// Happens in several phases:
//  - pools are allocated for the connection and request data structures
//  - events and logs are initialized
//  - ngx_connection_t object is created and initialized
//  - ngx_http_request_t object is created and initialized
//  - the upstream module is initialized with data to start the connection
//
// Comments on the individual phases are below.

// Allocates NGINX memory pools for data structures related to the connection
// and the request.
//
// Note: Two pools are currently used although because we now have full control
// over them (the subrequest code has less control over pools than we have
// here), a single pool could work as well.
Status allocate_pools(
    ngx_log_t *log,
    std::unique_ptr<ngx_pool_t, ngx_pool_t_deleter> *out_connection_pool,
    std::unique_ptr<ngx_pool_t, ngx_pool_t_deleter> *out_request_pool) {
  // Only these structures will be allocated from the pool. No need to allocate
  // pool bigger than necessary.
  const size_t kConnectionPoolSize = sizeof(ngx_pool_t) +
                                     sizeof(ngx_pool_cleanup_t) +
                                     sizeof(ngx_esp_http_connection);

  // Create the connection pool.
  std::unique_ptr<ngx_pool_t, ngx_pool_t_deleter> connection_pool(
      ngx_create_pool(kConnectionPoolSize, log));
  if (connection_pool == nullptr) {
    return Status(NGX_ERROR, "Out of memory");
  }

  // Allocate the request pool.
  std::unique_ptr<ngx_pool_t, ngx_pool_t_deleter> request_pool(
      ngx_create_pool(kRequestPoolSize, log));
  if (request_pool == nullptr) {
    return Status(NGX_ERROR, "Out of memory");
  }

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "created HTTP connection pool: %p",
                 connection_pool.get());
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "created HTTP request pool: %p",
                 request_pool.get());

  *out_connection_pool = std::move(connection_pool);
  *out_request_pool = std::move(request_pool);

  return Status::OK;
}

// Allocates ngx_http_request_t object we use for the HTTP request.
// Initializes header structures which require allocation.
ngx_http_request_t *allocate_ngx_http_request(ngx_pool_t *request_pool) {
  ngx_http_request_t *r = reinterpret_cast<ngx_http_request_t *>(
      ngx_pcalloc(request_pool, sizeof(ngx_http_request_t)));
  if (r == nullptr) {
    return nullptr;
  }

  // headers_out
  if (ngx_list_init(&r->headers_out.headers, request_pool, 20,
                    sizeof(ngx_table_elt_t)) != NGX_OK) {
    return nullptr;
  }

  // headers_in
  r->header_in = reinterpret_cast<ngx_buf_t *>(ngx_calloc_buf(request_pool));
  if (r->header_in == nullptr) {
    return nullptr;
  }
  r->header_in->last_buf = 1;
  r->header_in->last_in_chain = 1;

  return r;
}

// Initializes read and write events on the connection object events and logs.
// Log additionally requires setting up log context which points at the fake
// connection and the request object.
Status initialize_events_and_logs(ngx_esp_http_connection *http_connection) {
  // request must be initialized prior to this call!
  auto r = http_connection->request;
  if (r == nullptr) {
    return Status(NGX_ERROR, "Internal error");
  }

  // Set up read and write events. Their logs point at the pre-allocated log
  // structure.
  http_connection->read_event.log = &http_connection->log;
  http_connection->write_event.log = &http_connection->log;
  http_connection->write_event.active = 1;

  // Set up log context.
  // For connection we use the fake connection object, and we use the request
  // object for the log context's request context.
  http_connection->log_ctx.connection = &http_connection->connection;
  http_connection->log_ctx.request = r;
  http_connection->log_ctx.current_request = r;

  // Set up log object.
  http_connection->log.data = &http_connection->log_ctx;
  http_connection->log.file = ngx_cycle->new_log.file;

#if (NGX_DEBUG)
  http_connection->log.log_level = NGX_LOG_DEBUG_CONNECTION | NGX_LOG_DEBUG_ALL;
#else
  http_connection->log.log_level = ngx_cycle->new_log.log_level;
#endif

  return Status::OK;
}

// Initializes the ngx_connection_t field of the http connection.
// Most of the initialization is pointing connection's fields as other parts
// of the pre-allocated data structure (for example local_sockaddr). Only
// current request (stored in connection.data) must be created by this point.
Status initialize_connection(ngx_pool_t *connection_pool,
                             ngx_esp_http_connection *http_connection) {
  // request must be initialized prior to this call!
  auto r = http_connection->request;
  if (r == nullptr) {
    return Status(NGX_ERROR, "Internal error");
  }

  // Set up a local socket address used by the connection object.
  http_connection->local_sockaddr.sin_family = AF_INET;
  http_connection->local_sockaddr.sin_addr.s_addr =
      htonl((127 << 24) | (0 << 16) | (0 << 8) | 1);  // 127.0.0.1

  // Set up the connection.
  http_connection->connection.local_socklen =
      sizeof(http_connection->local_sockaddr);
  http_connection->connection.local_sockaddr =
      reinterpret_cast<sockaddr *>(&http_connection->local_sockaddr);
  http_connection->connection.read = &http_connection->read_event;
  http_connection->connection.write = &http_connection->write_event;

  // Set up the connection pool and its log.
  // The connection object will own the reference to the connection pool.
  http_connection->connection.pool = connection_pool;
  http_connection->connection.log = &http_connection->log;
  http_connection->connection.pool->log = &http_connection->log;
  http_connection->connection.log_error = NGX_ERROR_INFO;
  http_connection->connection.fd = (ngx_socket_t)-1;
  http_connection->connection.data = r;
  http_connection->connection.requests++;
  http_connection->connection.send_chain = esp_http_do_not_send_chain;

  return Status::OK;
}

// Initializes the ngx_request_t used to perform the HTTP request.
Status initialize_request(ngx_pool_t *request_pool,
                          ngx_esp_http_connection *http_connection) {
  // request must be initialized prior to this call!
  auto r = http_connection->request;
  if (r == nullptr) {
    return Status(NGX_ERROR, "Internal error");
  }

  // Set up the request pool and its log.
  // The request object will own the reference to the request pool.
  r->pool = request_pool;
  r->pool->log = &http_connection->log;

  // Set the request's configuration contexts.
  auto http_cctx = reinterpret_cast<ngx_http_conf_ctx_t *>(
      ngx_get_conf(ngx_cycle->conf_ctx, ngx_http_module));
  if (http_cctx == nullptr) {
    return Status(NGX_ERROR, "Internal error");
  }
  auto mc = reinterpret_cast<ngx_esp_main_conf_t *>(
      http_cctx->main_conf[ngx_esp_module.ctx_index]);
  if (mc == nullptr) {
    return Status(NGX_ERROR, "Internal error");
  }

  // The main and server configuration contexts we use are from the HTTP module.
  // The local configuration context is built from scratch at our module
  // initialization and is roughly equivelent in function to an empty location
  // configuration block.
  r->main_conf = http_cctx->main_conf;
  r->srv_conf = http_cctx->srv_conf;
  // Use the custom-built local configs.
  r->loc_conf = mc->http_module_conf_ctx.loc_conf;

  // Create per-module request context array.
  // Modules assign their contexts as they need during the request processing
  // so we only allocate an empty array here.
  r->ctx = reinterpret_cast<void **>(
      ngx_pcalloc(request_pool, sizeof(void *) * ngx_http_max_module));
  if (r->ctx == nullptr) {
    return Status(NGX_ERROR, "Out of memory");
  }

  // Allocate per-request variables.

  auto http_cmcf = reinterpret_cast<ngx_http_core_main_conf_t *>(
      ngx_http_get_module_main_conf(r, ngx_http_core_module));
  r->variables = reinterpret_cast<ngx_variable_value_t *>(ngx_pcalloc(
      request_pool,
      http_cmcf->variables.nelts * sizeof(ngx_http_variable_value_t)));
  if (r->variables == nullptr) {
    return Status(NGX_ERROR, "Out of memory");
  }

  // Set the request's connection.
  r->connection = &http_connection->connection;

  // Make the request its own main (our request is top-level).
  r->main = r;

  // Set count to 2 to prevent NGINX from trying to close the fake connection.
  // See ngx_http_close_request(ngx_http_request_t *r, ngx_int_t rc) where
  // there is:
  //
  //   r->count--;
  //
  //   if (r->count || r->blocked) {
  //       return;
  //   }
  //
  // followed by "close connection". We set count to 2, then we decrement to 1
  // and connection won't get closed.
  //
  r->count = 2;

  // Mark request as internal. This means, for example, that Endpoints
  // will ignore it.
  r->internal = 1;

  // Allocate a dummy ESP module context and set its field to http_connection
  ngx_esp_request_ctx_t *dummy = RegisterPoolCleanup(
      r->pool, new (r->pool) ngx_esp_request_ctx_t(r, nullptr));
  if (dummy == nullptr) {
    return Status(NGX_ERROR, "Out of memory");
  }

  dummy->http_subrequest = http_connection;

  // Set the request's ESP module context.
  ngx_http_set_ctx(r, dummy, ngx_esp_module);

  return Status::OK;
}

// Initializes the upstream data structures which NGINX upstream module uses to
// call the server.
Status initialize_upstream_request(ngx_log_t *log, HTTPRequest *request,
                                   ngx_pool_t *request_pool,
                                   ngx_esp_http_connection *http_connection) {
  // request must be initialized prior to this call!
  auto r = http_connection->request;
  if (r == nullptr) {
    return Status(NGX_ERROR, "Internal error");
  }

  // Create the NGINX upstream structures.
  if (ngx_http_upstream_create(r) != NGX_OK) {
    return Status(NGX_ERROR, "Out of memory");
  }

  ngx_http_upstream_t *upstream = r->upstream;

  // Parse the URL provided by the caller (stored on the HTTPRequest object).
  ngx_str_t url_str_t = ngx_null_string;
  if (ngx_str_copy_from_std(request_pool, request->url(), &url_str_t) !=
      NGX_OK) {
    return Status(NGX_ERROR, "Out of memory");
  }

  Status status = ngx_esp_upstream_set_url(request_pool, upstream, url_str_t,
                                           &http_connection->host_header,
                                           &http_connection->url_path);
  if (!status.ok()) {
    return status;
  }

  // Set timeout, defaulting to 60 seconds.
  //
  // NGINX has very fine-grained timeouts. We may want to further evolve
  // how we set timeouts for Endpoints.
  int timeout = request->timeout_ms();
  if (timeout <= 0) timeout = kDefaultTimeoutMilliseconds;

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "HTTP TIMEOUT: %d", timeout);

  http_connection->upstream_conf.buffering = 1;
  http_connection->upstream_conf.connect_timeout = timeout;
  http_connection->upstream_conf.read_timeout = timeout;
  http_connection->upstream_conf.send_timeout = timeout;
  http_connection->upstream_conf.buffer_size = ngx_pagesize;
  http_connection->upstream_conf.busy_buffers_size = 2 * ngx_pagesize;
  // This is the max response size: set it to 1MB
  // It means the largest service config ESP can download is 1MB.
  http_connection->upstream_conf.bufs.num = 256;
  http_connection->upstream_conf.bufs.size = ngx_pagesize;
  http_connection->upstream_conf.max_temp_file_size = 0;
  http_connection->upstream_conf.temp_file_write_size = 0;

  // pass_request_xxx doesn't apply to us because there is no (client)
  // request whose headers or body we would be passing along to the
  // upstream.
  http_connection->upstream_conf.pass_request_headers = 0;
  http_connection->upstream_conf.pass_request_body = 0;

  http_connection->upstream_conf.hide_headers =
      reinterpret_cast<ngx_array_t *>(NGX_CONF_UNSET_PTR);
  http_connection->upstream_conf.pass_headers =
      reinterpret_cast<ngx_array_t *>(NGX_CONF_UNSET_PTR);

// Set up SSL if available and required.
#if NGX_HTTP_SSL
  if (upstream->ssl) {
    auto http_cctx = reinterpret_cast<ngx_http_conf_ctx_t *>(
        ngx_get_conf(ngx_cycle->conf_ctx, ngx_http_module));
    auto mc = reinterpret_cast<ngx_esp_main_conf_t *>(
        http_cctx->main_conf[ngx_esp_module.ctx_index]);
    http_connection->upstream_conf.ssl = mc->ssl;
    http_connection->upstream_conf.ssl_session_reuse = 1;
    // For SNI (Server Name Indication) support
    http_connection->upstream_conf.ssl_server_name = 1;
    if (mc->cert_path.len > 0) {
      http_connection->upstream_conf.ssl_verify = 1;
    }
  }
#endif

  upstream->output.tag = (ngx_buf_tag_t)&ngx_esp_module;
  upstream->conf = &http_connection->upstream_conf;
  upstream->buffering = 1;

  // Set up the upstream handlers which create the request HTTP buffers, and
  // process the response data as the upstream module reads it from the wire.

  // The request filter context is the request object (ngx_request_t), from
  // which we can access ngx_esp_http_connection.
  upstream->input_filter_ctx = r;
  upstream->input_filter_init = ngx_esp_upstream_input_filter_init;
  upstream->input_filter = ngx_esp_upstream_input_filter;
  upstream->create_request = ngx_esp_upstream_create_request;
  upstream->reinit_request = ngx_esp_upstream_reinit_request;
  upstream->process_header = ngx_esp_upstream_process_status_line;
  upstream->abort_request = ngx_esp_upstream_abort_request;
  upstream->finalize_request = ngx_esp_upstream_finalize_request;

  // Fix for 1.15.0 compatibility issue
  upstream->pipe =
      (ngx_event_pipe_t *)ngx_pcalloc(request_pool, sizeof(ngx_event_pipe_t));
  if (upstream->pipe == NULL) {
    return Status(NGX_ERROR, "Out of memory");
  }

  upstream->pipe->input_filter = ngx_esp_upstream_pipe_input_filter;
  upstream->pipe->input_ctx = r;
  upstream->accel = 1;

  return Status::OK;
}

// Creates data structures necessary to use NGINX upstream module as an HTTP
// client.
Status ngx_esp_create_http_request(
    ngx_log_t *log, HTTPRequest *request,
    ngx_esp_http_connection **out_http_connection) {
  // Create the connection pool and request pools.
  std::unique_ptr<ngx_pool_t, ngx_pool_t_deleter> connection_pool;
  std::unique_ptr<ngx_pool_t, ngx_pool_t_deleter> request_pool;

  Status status = allocate_pools(log, &connection_pool, &request_pool);
  if (!status.ok()) {
    return status;
  }

  // Allocate the HTTP connection state in one go.
  // Because we custom-fit the pool size to match this one allocation, the
  // alloction will not fail.
  ngx_esp_http_connection *http_connection =
      RegisterPoolCleanup(connection_pool.get(), new (connection_pool.get())
                                                     ngx_esp_http_connection());
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                 "esp: allocated http_connection %p", http_connection);
  if (http_connection == nullptr) {
    return Status(NGX_ERROR, "Out of memory");
  }

  // Allocate and initialize an ngx_http_request_t.
  http_connection->request = allocate_ngx_http_request(request_pool.get());

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "esp: allocated http request %p",
                 http_connection->request);

  if (http_connection->request == nullptr) {
    return Status(NGX_ERROR, "Out of memory");
  }

  // Initialize event and log structures.
  status = initialize_events_and_logs(http_connection);
  if (!status.ok()) {
    return status;
  }

  // Initialize the ngx_connection_t structure.
  status = initialize_connection(connection_pool.get(), http_connection);
  if (!status.ok()) {
    return status;
  }

  // Initialize the ngx_request_t structure.
  status = initialize_request(request_pool.get(), http_connection);
  if (!status.ok()) {
    return status;
  }

  // Initialize the upstream structures which the upstream module uses to
  // connect to the server.
  status = initialize_upstream_request(log, request, request_pool.get(),
                                       http_connection);
  if (!status.ok()) {
    return status;
  }

  // Success. Release the pool smart pointers.
  //
  // The pools are now owned by the connection and request objects
  // stored on the http_connection.
  //
  // Specifically:
  //     request pool: http_connection->request->pool
  //     connection pool: http_connection->connection.pool
  //
  // They will be destroyed in wakeup_event_handler.
  connection_pool.release();
  request_pool.release();

  // Return the HTTP connection object to the caller.
  *out_http_connection = http_connection;

  return Status::OK;
}

}  // namespace

void ngx_esp_send_http_request(std::unique_ptr<HTTPRequest> request) {
  ngx_esp_http_connection *http_connection(nullptr);

  ngx_log_t *log = ngx_cycle->log;

  ngx_log_debug3(NGX_LOG_DEBUG_HTTP, log, 0,
                 "ESP: sending http request: %s %s\n body size %d",
                 request->method().c_str(), request->url().c_str(),
                 request->body().size());

  // Create the HTTP request data structures.
  Status status =
      ngx_esp_create_http_request(log, request.get(), &http_connection);

  if (status.ok()) {
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "esp: calling ngx_http_upstream_init(%p)",
                   http_connection->request);

    // Store the caller's request for the continuation call.
    http_connection->esp_request = std::move(request);

    // Initiate the upstream connection by calling NGINX upstream.
    ngx_http_upstream_init(http_connection->request);
  } else {
    status = Status(NGX_ERROR,
                    "Unable to initiate HTTP request: " + status.message());

    // Call the request continuation with error.
    request->OnComplete(status, std::map<std::string, std::string>(), "");
  }
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
