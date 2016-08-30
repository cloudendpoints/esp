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
#ifndef API_MANAGER_SERVICE_CONTROL_INFO_H_
#define API_MANAGER_SERVICE_CONTROL_INFO_H_

#include "google/protobuf/stubs/stringpiece.h"

#include <string.h>
#include <memory>
#include <string>

#include "include/api_manager/compute_platform.h"
#include "include/api_manager/protocol.h"
#include "include/api_manager/service_control.h"
#include "include/api_manager/utils/status.h"

namespace google {
namespace api_manager {
namespace service_control {

// Use the CheckRequestInfo and ReportRequestInfo to fill Service Control
// request protocol buffers. Use following two structures to pass
// in minimum info and call Fill functions to fill the protobuf.

// Basic information about the API call (operation).
struct OperationInfo {
  // The service name
  ::google::protobuf::StringPiece service_name;

  // Identity of the operation. It must be unique within the scope of the
  // service. If the service calls Check() and Report() on the same operation,
  // the two calls should carry the same operation id.
  ::google::protobuf::StringPiece operation_id;

  // Fully qualified name of the operation.
  ::google::protobuf::StringPiece operation_name;

  // The producer project id.
  ::google::protobuf::StringPiece producer_project_id;

  // The API key.
  ::google::protobuf::StringPiece api_key;

  // Uses Referer header, if the Referer header doesn't present, use the
  // Origin header. If both of them not present, it's empty.
  ::google::protobuf::StringPiece referer;

  OperationInfo() {}
};

// Information to fill Check request protobuf.
struct CheckRequestInfo : public OperationInfo {
  // The client IP address.
  std::string client_ip;
};

// Stores the information substracted from the check response.
struct CheckResponseInfo {
  // If the information of this struct is valid.
  bool valid;
  // If the request have a valid api key.
  bool is_api_key_valid;
  // If service is activated.
  bool service_is_activated;

  CheckResponseInfo() : valid(false) {}
};

// Information to fill Report request protobuf.
struct ReportRequestInfo : public OperationInfo {
  // The HTTP response code.
  int response_code;

  // The response status.
  utils::Status status;

  // Original request URL.
  std::string url;

  // location of the service, such as us-central.
  ::google::protobuf::StringPiece location;
  // API name and version.
  ::google::protobuf::StringPiece api_name;
  // API full method name.
  ::google::protobuf::StringPiece api_method;

  // The request size in bytes. -1 if not available.
  int64_t request_size;

  // The response size in bytes. -1 if not available.
  int64_t response_size;

  // per request latency.
  LatencyInfo latency;

  // The message to log as INFO log.
  std::string log_message;

  // Auth info: issuer and audience.
  std::string auth_issuer;
  std::string auth_audience;

  // Protocol used to issue the request.
  protocol::Protocol protocol;

  // HTTP method. all-caps string such as "GET", "POST" etc.
  std::string method;

  // A recognized compute platform (GAE, GCE, GKE).
  compute_platform::ComputePlatform compute_platform;

  // If consumer data should be sent.
  CheckResponseInfo check_response_info;

  ReportRequestInfo()
      : response_code(200),
        status(utils::Status::OK),
        request_size(-1),
        response_size(-1),
        protocol(protocol::UNKNOWN),
        compute_platform(compute_platform::UNKNOWN) {}
};

}  // namespace service_control
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_SERVICE_CONTROL_INFO_H_
