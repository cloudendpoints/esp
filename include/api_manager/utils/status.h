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
#ifndef API_MANAGER_UTILS_STATUS_H_
#define API_MANAGER_UTILS_STATUS_H_

#include <string>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/stubs/status.h"
#include "google/rpc/status.pb.h"

using ::google::protobuf::util::error::Code;

namespace google {
namespace api_manager {
namespace utils {

// A Status object can be used to represent an error or an OK state. Error
// status messages have an error code, an error message, and an error cause.
// An OK status has a code of 0 or 200 and no message.
class Status final {
 public:
  enum ErrorCause {
    // Internal proxy error (default)
    INTERNAL = 0,
    // External application error
    APPLICATION = 1,
    // Error in authentication
    AUTH = 2,
    // Error in service control check
    SERVICE_CONTROL = 3
  };

  // Constructs a status with an error code and message. If code == 0
  // message is ignored and a Status object identical to Status::OK
  // is constructed. Error cause is optional and defaults to INTERNAL.
  Status(int code, const std::string& message);
  Status(int code, const std::string& message, ErrorCause error_cause);
  ~Status() {}

  bool operator==(const Status& x) const;
  bool operator!=(const Status& x) const { return !operator==(x); }

  // Constructs a Status object from a protobuf Status.
  static Status FromProto(const ::google::protobuf::util::Status& proto_status);

  // The predefined OK status object.
  static const Status& OK;

  // Returns a representation of the error as a protobuf Status.
  ::google::protobuf::util::Status ToProto() const;

  // Returns true if this status is not an error (code == 0).
  bool ok() const { return code_ == 0 || code_ == 200; }

  // Returns the error code held by this status.
  int code() const { return code_; }

  // Returns the error message held by this status.
  const std::string& message() const { return message_; }

  // Sets the message stored in this error.
  void SetMessage(const std::string& message) { message_ = message; }

  // Returns the error cause held by this status.
  ErrorCause GetErrorCause() const { return error_cause_; }

  // Update the error cause held by this status.
  void SetErrorCause(ErrorCause error_cause) { error_cause_ = error_cause; }

  // Returns the external string representation of the error cause
  std::string GetErrorCauseString() const {
    switch (error_cause_) {
      default:
      case INTERNAL:
        return "internal";
      case APPLICATION:
        return "application";
      case AUTH:
        return "auth";
      case SERVICE_CONTROL:
        return "service_control";
    }
  }

  // Returns the error code mapped to HTTP status codes.
  int HttpCode() const;

  // Returns the error code mapped to protobuf canonical code.
  Code CanonicalCode() const;

  // Returns a JSON representation of the error as a std::string, with the
  // following format:
  // {
  //   error: {
  //     code: <http status code>,
  //     status: <canonical status code>,
  //     message: <error message>,
  //   }
  // }
  std::string ToJson() const;

  // Returns a combination of the error code name and message.
  std::string ToString() const;

 private:
  // Constructs the OK status.
  Status();

  // Error code. Zero means OK. Negative numbers are for control
  // statuses (e.g. DECLINED). Positive numbers below 100 represent grpc
  // status codes. Positive numbers 100 and greater represent HTTP status codes.
  int code_;

  // Error cause indicating the origin of the error.
  ErrorCause error_cause_;

  // The error message if this Status represents an error, otherwise an empty
  // string if this is the OK status.
  std::string message_;
};

}  // namespace utils
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_UTILS_STATUS_H_
