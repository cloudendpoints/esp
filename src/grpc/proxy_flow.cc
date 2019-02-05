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
#include "src/grpc/proxy_flow.h"

#include "grpc/grpc.h"
#include "grpc/support/alloc.h"
#include "include/api_manager/utils/status.h"
#include "src/core/lib/slice/b64.h"

using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::UNAVAILABLE;
using ::google::protobuf::util::error::UNKNOWN;
using std::chrono::system_clock;

namespace google {
namespace api_manager {
namespace grpc {

// Some notes about ProxyFlows:
//
// There can be any number of asynchronous operations in flight on
// behalf of a ProxyFlow at any given time.  Each logical operation needs
// to maintain a reference to the ProxyFlow, so that the ProxyFlow sticks around
// until the operation completes.
//
// Items in a GRPC completion queue are tracked by tags; in the GRPC
// proxy server, the tags are pointers to std::function objects, which
// are run when the tag is dequeued.
//
// So we implement the logical reference tracking in the simplest
// possible way: each pending tag holds, as part of its state, a
// std::shared_ptr<ProxyFlow>, keeping the ProxyFlow alive until the tag
// function completes.
//
// A side effect of this is that the lambdas used to create the
// callbacks never capture "this".  Instead, they capture a
// std::shared_ptr<ProxyFlow>, typically named "flow", in order to preserve
// the reference.
//
//   Aside: Another way to implement this reference tracking would've
//          been to represent each outstanding operation as an object,
//          and to have the tag functions maintain a std::unique_ptr
//          to those objects, which would have their own
//          std::shared_ptr references to the ProxyFlow.  This would
//          involve a bit more code, but would have the nice benefit
//          of clearly associating each function with the logical
//          operation it's performing (e.g. clearly separating the
//          upstream->downstream path from the downstream->upstream
//          path).  We might consider that refactoring at some point.
//
// The actual control flow sequence looks like this:
//
//                UpstreamCall
//                     +
//            [success]|
//                     |
//   +-----------------+-----------+
//   |                             |
//   |                             |
//   |      +-----------------+    |
//   |      |                 |    |
//   |      |                 v    v
//   |      |              DownstreamReadMessage
//   |      |                      +
//   |      |                      |
//   |      |            [message] | [end of msgs]
//   |      +           +----------+-----------------+
//   |      |           |                            |
//   |      |           v                            v
//   |      | UpstreamWriteMessage         UpstreamWritesDone
//   |      |           |                            +
//   |      +-----------+                            |[success]
//   |                                               v
//   |                                              +-+
//   |                                              +-+
//   |
//   |
//   v
// UpstreamReadInitialMetadata
//   +
//   |[success]
//   |
//   +------------------------------------+         +-----------+
//   |                                    |         |           |
//   v                                    v         v           |
// DownstreamWriteInitialMetadata    UpstreamReadMessage        |
//   +                                    +                     |
//   |[success]                           |                     |
//   v                      [end of msgs] | [message]           |
//  +-+                 +-----------------+---------+           |
//  +-+                 |                           |           |
//                      v                           v           |
//                UpstreamFinish         DownstreamWriteMessage |
//                      +                           |           |
//                      |                           +-----------+
//                      v
//               DownstreamFinish
//
// All transitions labeled [success] also define an implicit
// transition to "DownstreamFinish" in case of error.

namespace {

const char kGrpcEncoding[] = "grpc-encoding";
const char kGrpcAcceptEncoding[] = "grpc-accept-encoding";
const char kGrpcHostHeader[] = "host";

Status ProcessDownstreamHeaders(
    const std::multimap<std::string, std::string> &headers,
    ::grpc::ClientContext *context) {
  for (const auto &it : headers) {
    // gRPC requests (HTTP2) with a host header will lead some gRPC servers to
    // reject it, so the host header is skipped here.
    if (it.first == kGrpcEncoding || it.first == kGrpcAcceptEncoding ||
        it.first == kGrpcHostHeader) {
      // GRPC lib will add this header, so not adding it to client_context_
      continue;
    }
    // GRPC runtime libraries use "-bin" suffix to detect binary headers and
    // properly apply base64 encoding & decoding as headers are sent and
    // received. So we decode here before passing it to GRPC runtime.
    if (grpc_is_binary_header(::grpc::SliceReferencingString(it.first))) {
      // Workaround for https://github.com/grpc/grpc/issues/8624
      if (it.second.length() == 0) {
        continue;
      }
      ::grpc::Slice value_slice(
          grpc_base64_decode_with_len(it.second.c_str(), it.second.length(),
                                      false),
          ::grpc::Slice::STEAL_REF);
      std::string binary_value(
          reinterpret_cast<const char *>(value_slice.begin()),
          value_slice.size());
      context->AddMetadata(it.first, std::move(binary_value));
    } else {
      context->AddMetadata(it.first, it.second);
    }
  }
  return Status::OK;
}

Status ProcessUpstreamHeaders(
    const std::multimap<::grpc::string_ref, ::grpc::string_ref>
        &upstream_headers,
    std::multimap<std::string, std::string> *downstream_headers) {
  for (auto &it : upstream_headers) {
    std::string key(it.first.data(), it.first.size());
    std::string value;
    if (grpc_is_binary_header(::grpc::SliceReferencingString(key))) {
      char *b64_value =
          grpc_base64_encode(it.second.data(), it.second.size(), 0, 0);
      if (b64_value == nullptr) {
        continue;
      }

      // grpc_base64_encode may have added padding. If not needed, remove them.
      size_t len = strlen(b64_value);
      while (len > 0 && b64_value[len - 1] == '=') {
        len--;
      }
      b64_value[len] = '\0';

      value = b64_value;
      gpr_free(b64_value);
    } else {
      value = std::string(it.second.data(), it.second.size());
    }
    downstream_headers->emplace(std::move(key), std::move(value));
  }
  return Status::OK;
}
}  // namespace

void ProxyFlow::Start(AsyncGrpcQueue *async_grpc_queue,
                      std::shared_ptr<ServerCall> server_call,
                      std::shared_ptr<::grpc::GenericStub> upstream_stub,
                      const std::string &method,
                      const std::multimap<std::string, std::string> &headers) {
  auto flow = std::make_shared<ProxyFlow>(
      async_grpc_queue, std::move(server_call), upstream_stub);
  Status status = ProcessDownstreamHeaders(headers, &flow->upstream_context_);
  if (status.ok()) {
    ProxyFlow::StartUpstreamCall(flow, method);
  } else {
    ProxyFlow::StartDownstreamFinish(flow, status);
  }
}

ProxyFlow::ProxyFlow(AsyncGrpcQueue *async_grpc_queue,
                     std::shared_ptr<ServerCall> server_call,
                     std::shared_ptr<::grpc::GenericStub> upstream_stub)
    : sent_upstream_writes_done_(false),
      started_upstream_finish_(false),
      sent_downstream_finish_(false),
      async_grpc_queue_(async_grpc_queue),
      server_call_(std::move(server_call)),
      upstream_stub_(std::move(upstream_stub)),
      status_from_esp_(Status::OK) {}

Status ProxyFlow::StatusFromGRPCStatus(const ::grpc::Status &status) {
  // The GRPC error code space happens to match the protocol buffer
  // canonical code space used by ESP Status, so this is a pretty
  // simple translation.
  return Status(status.error_code(), status.error_message());
}

void ProxyFlow::StartUpstreamCall(std::shared_ptr<ProxyFlow> flow,
                                  const std::string &method) {
  // Note: the lock must be held around the Call call, since it's
  // important that the callback not attempt to use
  // upstream_reader_writer_ until it's been initialized.
  // Fortunately, the callback completion function runs
  // asynchronously.
  flow->start_time_ = system_clock::now();
  std::lock_guard<std::mutex> lock(flow->mu_);
  flow->upstream_reader_writer_ = flow->upstream_stub_->Call(
      &flow->upstream_context_, method, flow->async_grpc_queue_->GetQueue(),
      flow->async_grpc_queue_->MakeTag([flow](bool ok) {
        if (!ok) {
          StartDownstreamFinish(
              flow,
              Status(UNAVAILABLE, std::string("upstream backend unavailable")));
          return;
        }
        StartUpstreamReadInitialMetadata(flow);
        StartDownstreamReadMessage(flow);
      }));
}

void ProxyFlow::StartDownstreamReadMessage(std::shared_ptr<ProxyFlow> flow) {
  {
    std::lock_guard<std::mutex> lock(flow->mu_);
    if (flow->sent_downstream_finish_) {
      return;
    }
  }
  flow->server_call_->Read(&flow->downstream_to_upstream_buffer_,
                           [flow](bool proceed, utils::Status status) {
                             if (proceed) {
                               StartUpstreamWriteMessage(
                                   flow, status == Status::DONE);
                             } else {
                               StartUpstreamWritesDone(flow, status);
                             }
                           });
}

void ProxyFlow::StartUpstreamWritesDone(std::shared_ptr<ProxyFlow> flow,
                                        utils::Status status) {
  {
    std::lock_guard<std::mutex> lock(flow->mu_);

    // NOTE: For gRPC steaming, at this point client-side streaming is done, but
    // server-side streaming is not. There needs to be an additional mechanism
    // to detect RST_STREAM and finish upstream properly. This problem can arise
    // for server-side streaming when ESP is connected with connection pooling
    // and multiple requests share a single HTTP connection.
    flow->server_call_->SetCancel([flow]() {
      flow->upstream_context_.TryCancel();
      StartUpstreamFinish(flow);
    });

    if (flow->sent_upstream_writes_done_) {
      return;
    }
    flow->sent_upstream_writes_done_ = true;
  }
  flow->upstream_reader_writer_->WritesDone(
      flow->async_grpc_queue_->MakeTag([flow, status](bool ok) {
        if (!ok) {
          // Upstream is not writable, call finish to get status and
          // and finish the call
          StartUpstreamFinish(flow);
          return;
        }
        if (!status.ok()) {
          StartDownstreamFinish(flow, status);
          return;
        }
      }));
}

void ProxyFlow::StartUpstreamWriteMessage(std::shared_ptr<ProxyFlow> flow,
                                          bool last) {
  ::grpc::WriteOptions options;
  if (last) {
    options.set_last_message();
    std::lock_guard<std::mutex> lock(flow->mu_);
    if (flow->sent_upstream_writes_done_) {
      return;
    }
    flow->sent_upstream_writes_done_ = true;
  }
  flow->server_call_->UpdateRequestMessageStat(
      static_cast<int64_t>(flow->downstream_to_upstream_buffer_.Length()));
  flow->upstream_reader_writer_->Write(
      flow->downstream_to_upstream_buffer_, options,
      flow->async_grpc_queue_->MakeTag([flow](bool ok) {
        flow->downstream_to_upstream_buffer_.Clear();
        if (!ok) {
          // Upstream is not writable, call finish to get status and
          // and finish the call
          StartUpstreamFinish(flow);
          return;
        }
        // Now that the write has completed, it's safe to start the
        // next read.
        StartDownstreamReadMessage(flow);
      }));
}

void ProxyFlow::StartUpstreamReadInitialMetadata(
    std::shared_ptr<ProxyFlow> flow) {
  {
    std::lock_guard<std::mutex> lock(flow->mu_);
    if (flow->sent_downstream_finish_) {
      return;
    }
  }
  flow->upstream_reader_writer_->ReadInitialMetadata(
      flow->async_grpc_queue_->MakeTag([flow](bool ok) {
        if (!ok) {
          StartDownstreamFinish(
              flow,
              Status(UNKNOWN,
                     std::string("upstream backend failed to send metadata")));
          return;
        }
        StartDownstreamWriteInitialMetadata(flow);
        StartUpstreamReadMessage(flow);
      }));
}

void ProxyFlow::StartDownstreamWriteInitialMetadata(
    std::shared_ptr<ProxyFlow> flow) {
  std::multimap<std::string, std::string> initial_metadata;
  {
    std::lock_guard<std::mutex> lock(flow->mu_);
    if (flow->sent_downstream_finish_) {
      return;
    }
    ProcessUpstreamHeaders(flow->upstream_context_.GetServerInitialMetadata(),
                           &initial_metadata);
  }
  flow->server_call_->SendInitialMetadata(initial_metadata, [flow](bool ok) {
    if (!ok) {
      StartDownstreamFinish(
          flow,
          Status(UNKNOWN, std::string("failed to send initial metadata")));
    }
  });
}

void ProxyFlow::StartUpstreamReadMessage(std::shared_ptr<ProxyFlow> flow) {
  {
    std::lock_guard<std::mutex> lock(flow->mu_);
    if (flow->sent_downstream_finish_) {
      return;
    }
  }
  flow->upstream_reader_writer_->Read(
      &flow->upstream_to_downstream_buffer_,
      flow->async_grpc_queue_->MakeTag([flow](bool ok) {
        if (!ok) {
          StartUpstreamFinish(flow);
          return;
        }
        StartDownstreamWriteMessage(flow);
      }));
}

void ProxyFlow::StartDownstreamWriteMessage(std::shared_ptr<ProxyFlow> flow) {
  {
    std::lock_guard<std::mutex> lock(flow->mu_);
    if (flow->sent_downstream_finish_) {
      return;
    }
  }
  flow->server_call_->UpdateResponseMessageStat(
      static_cast<int64_t>(flow->upstream_to_downstream_buffer_.Length()));
  flow->server_call_->Write(
      flow->upstream_to_downstream_buffer_, [flow](bool ok) {
        if (!ok) {
          StartDownstreamFinish(
              flow,
              Status(UNKNOWN,
                     std::string(
                         "failed to send a message to the downstream client")));
          return;
        }
        StartUpstreamReadMessage(flow);
      });
}

void ProxyFlow::StartUpstreamFinish(std::shared_ptr<ProxyFlow> flow) {
  {
    std::lock_guard<std::mutex> lock(flow->mu_);
    if (flow->started_upstream_finish_) {
      return;
    }
    flow->started_upstream_finish_ = true;
  }
  flow->upstream_reader_writer_->Finish(
      &flow->status_from_upstream_,
      flow->async_grpc_queue_->MakeTag(
          [flow](bool ok) { StartDownstreamFinish(flow, Status::OK); }));
}

void ProxyFlow::StartDownstreamFinish(std::shared_ptr<ProxyFlow> flow,
                                      Status status) {
  {
    std::lock_guard<std::mutex> lock(flow->mu_);
    if (flow->sent_downstream_finish_) {
      return;
    }
    flow->sent_downstream_finish_ = true;
  }

  flow->status_from_esp_ = status;

  if (status.ok()) {
    status = StatusFromGRPCStatus(flow->status_from_upstream_);
  }

  std::multimap<std::string, std::string> response_trailers;

  if (flow->status_from_esp_.ok()) {
    ProcessUpstreamHeaders(flow->upstream_context_.GetServerTrailingMetadata(),
                           &response_trailers);
  }
  int64_t backend_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                             system_clock::now() - flow->start_time_)
                             .count();
  flow->server_call_->RecordBackendTime(backend_time);
  flow->server_call_->Finish(status, std::move(response_trailers));
}

}  // namespace grpc
}  // namespace api_manager
}  // namespace google
