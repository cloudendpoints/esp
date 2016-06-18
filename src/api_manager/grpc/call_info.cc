// Copyright (C) Endpoints Server Proxy Authors
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
#include "src/api_manager/grpc/call_info.h"

#include <utility>

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {
namespace grpc {

CallInfo::CallInfo(::grpc::GenericServerContext *context)
    : peer_(context->peer()),
      method_(context->method()),
      request_size_(0),
      response_size_(0),
      status_(Status::OK) {
  for (const auto &it : context->client_metadata()) {
    std::string key(it.first.data(), it.first.size());
    std::string value(it.second.data(), it.second.size());
    request_headers_.emplace(std::move(key), std::move(value));
  }
}

std::string CallInfoRequest::GetRequestHTTPMethod() {
  // For GRPC calls, the method is always POST.
  static const std::string post = "POST";
  return post;
}

std::string CallInfoRequest::GetClientIP() {
  // Set the remote IP address, by taking the peer string and
  // stripping off the protocol (at the beginning) and the port (at
  // the end).
  std::string peer = call_info_->GetPeer();
  std::string::size_type first_sep = peer.find(':');
  std::string::size_type last_sep = peer.rfind(':');
  if (first_sep != last_sep && first_sep != std::string::npos &&
      last_sep != std::string::npos) {
    return peer.substr(first_sep + 1, last_sep - first_sep - 1);
  } else {
    return std::string();
  }
}

bool CallInfoRequest::FindHeader(const std::string &name, std::string *header) {
  for (const auto &it : *call_info_->GetRequestHeaders()) {
    if (strcasecmp(name.c_str(), it.first.c_str()) == 0) {
      *header = it.second;
      return true;
    }
  }
  return false;
}

}  // namespace grpc
}  // namespace api_manager
}  // namespace google
