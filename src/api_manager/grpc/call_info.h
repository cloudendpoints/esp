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
#ifndef API_MANAGER_GRPC_CALL_INFO_H_
#define API_MANAGER_GRPC_CALL_INFO_H_

#include <map>
#include <string>

#include "grpc++/generic/async_generic_service.h"
#include "include/api_manager/request.h"
#include "include/api_manager/response.h"

namespace google {
namespace api_manager {
namespace grpc {

// Stores information for a call.
class CallInfo {
 public:
  explicit CallInfo(::grpc::GenericServerContext *context);
  virtual ~CallInfo() {}

  void SetStatus(utils::Status status) { status_ = status; }
  utils::Status GetStatus() const { return status_; }

  void AddRequestSize(size_t length) { request_size_ += length; }
  void AddResponseSize(size_t length) { response_size_ += length; }
  std::size_t GetRequestSize() const { return request_size_; }
  std::size_t GetResponseSize() const { return response_size_; }

  std::string GetMethod() const { return method_; }
  std::string GetPeer() const { return peer_; }

  std::multimap<std::string, std::string> *GetRequestHeaders() {
    return &request_headers_;
  }
  std::multimap<std::string, std::string> *GetResponseHeaders() {
    return &response_headers_;
  }
  std::multimap<std::string, std::string> *GetResponseTrailers() {
    return &response_trailers_;
  }

 private:
  std::multimap<std::string, std::string> request_headers_;
  std::multimap<std::string, std::string> response_headers_;
  std::multimap<std::string, std::string> response_trailers_;
  std::string peer_;
  std::string method_;
  std::size_t request_size_;
  std::size_t response_size_;
  utils::Status status_;
};

// Implements ESP Request interface.
class CallInfoRequest : public Request {
 public:
  explicit CallInfoRequest(CallInfo *call_info) : call_info_(call_info) {}
  virtual ~CallInfoRequest() {}

  virtual std::string GetRequestHTTPMethod();

  virtual std::string GetRequestPath() { return call_info_->GetMethod(); }
  virtual protocol::Protocol GetRequestProtocol() { return protocol::GRPC; }
  virtual std::string GetUnparsedRequestPath() { return std::string(); }

  virtual std::string GetClientIP();

  virtual bool FindQuery(const std::string &name, std::string *query) {
    return false;
  }
  virtual bool FindHeader(const std::string &name, std::string *header);

  virtual utils::Status AddHeaderToBackend(const std::string &name,
                                           const std::string &value) {
    return utils::Status::OK;
  }
  virtual void SetAuthToken(const std::string &auth_token) {}

 private:
  CallInfo *call_info_;
};

// Implements ESP Response interface.
class CallInfoResponse : public Response {
 public:
  explicit CallInfoResponse(CallInfo *call_info) : call_info_(call_info) {}
  virtual ~CallInfoResponse() {}

  virtual utils::Status GetResponseStatus() { return call_info_->GetStatus(); }
  virtual std::size_t GetRequestSize() { return call_info_->GetRequestSize(); }
  virtual std::size_t GetResponseSize() {
    return call_info_->GetResponseSize();
  }

  virtual utils::Status GetLatencyInfo(service_control::LatencyInfo *info) {
    return utils::Status::OK;
  }

 private:
  CallInfo *call_info_;
};

}  // namespace grpc
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_GRPC_CALL_INFO_H_
