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
#include "src/api_manager/grpc/grpc_server_call.h"

#include "src/api_manager/grpc/proxy_flow.h"

using ::google::protobuf::util::error::INTERNAL;
using ::google::protobuf::util::error::UNAVAILABLE;
using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {
namespace grpc {

GrpcServerCall::GrpcServerCall(std::shared_ptr<Server> server)
    : server_(server),
      downstream_context_(),
      call_handler_(server_->call_handler_factory_()) {}

GrpcServerCall::~GrpcServerCall() { server_->CallComplete(); }

void GrpcServerCall::Start(std::shared_ptr<GrpcServerCall> server_call) {
  server_call->CallAccepted();
  server_call->call_handler_->Check(
      std::unique_ptr<Request>(new CallInfoRequest(server_call->call_info())),
      [server_call](std::string backend, Status status) {
        std::string status_str;
        if (!status.ok()) {
          status_str = status.ToString();
        }
        if (!status.ok()) {
          server_call->Finish(status);
          return;
        }
        if (server_call->server_->backend_override_.length()) {
          backend = server_call->server_->backend_override_;
        }
        if (!backend.length()) {
          server_call->Finish(
              Status(INTERNAL, std::string("no upstream backend configured")));
          return;
        }
        auto upstream_stub = server_call->server_->stub_map()->GetStub(backend);
        if (!upstream_stub) {
          server_call->Finish(Status(
              UNAVAILABLE, std::string("unable to connect to backend server")));
          return;
        }
        ProxyFlow::Start(server_call->server_.get(), server_call,
                         std::move(upstream_stub),
                         server_call->downstream_context_.method(),
                         *server_call->call_info_->GetRequestHeaders());
      });
}

void GrpcServerCall::AddInitialMetadata(std::string key, std::string value) {
  downstream_context_.AddInitialMetadata(std::move(key), std::move(value));
}

void GrpcServerCall::SendInitialMetadata(
    std::function<void(bool)> continuation) {
  downstream_reader_writer_.SendInitialMetadata(server_->MakeTag(continuation));
}

void GrpcServerCall::Read(::grpc::ByteBuffer *msg,
                          std::function<void(bool)> continuation) {
  auto server_call = GetPtr();
  downstream_reader_writer_.Read(
      msg, server_->MakeTag([server_call, msg, continuation](bool ok) {
        if (ok) {
          server_call->call_info_->AddRequestSize(msg->Length());
        }
        continuation(ok);
      }));
}

void GrpcServerCall::Write(const ::grpc::ByteBuffer &msg,
                           std::function<void(bool)> continuation) {
  auto server_call = GetPtr();
  downstream_reader_writer_.Write(
      msg, server_->MakeTag([server_call, msg, continuation](bool ok) {
        if (ok) {
          server_call->call_info_->AddResponseSize(msg.Length());
        }
        continuation(ok);
      }));
}

void GrpcServerCall::Finish(
    const Status &status,
    std::multimap<std::string, std::string> response_trailers) {
  auto server_call = GetPtr();

  call_info_->SetStatus(status);
  call_info_->AddResponseSize(status.message().length());
  *(call_info_->GetResponseTrailers()) = std::move(response_trailers);

  call_handler_->Report(
      std::unique_ptr<Response>(new CallInfoResponse(server_call->call_info())),
      [server_call]() {
        // This just exists to maintain a shared_ptr reference
        // to server_call for the duration of the Report
        // sequence.
      });

  for (const auto &it : *call_info_->GetResponseTrailers()) {
    downstream_context_.AddTrailingMetadata(it.first, it.second);
  }

  ::grpc::Status grpc_status(
      static_cast<::grpc::StatusCode>(status.CanonicalCode()),
      status.message());

  downstream_reader_writer_.Finish(grpc_status,
                                   server_->MakeTag([server_call](bool ok) {
                                     // This just exists to maintain a
                                     // shared_ptr reference to
                                     // server_call for the duration of the
                                     // Finish call.
                                   }));
}

}  // namespace grpc
}  // namespace api_manager
}  // namespace google
