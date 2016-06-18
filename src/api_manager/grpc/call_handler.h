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
#ifndef API_MANAGER_GRPC_CALL_HANDLER_H_
#define API_MANAGER_GRPC_CALL_HANDLER_H_

#include <functional>
#include <map>
#include <string>

#include "include/api_manager/request.h"
#include "include/api_manager/response.h"
#include "include/api_manager/utils/status.h"

namespace google {
namespace api_manager {
namespace grpc {

// CallHandler is an interface wraping around the ESP's RequestHandler
// interface. It provides a flexibility to plugin different ESP running in
// different environments.
// Grpc_proxy is running in its own thread and it imposes some thread
// requirements on the ESP environments. For example, if an ESP is
// running in the Nginx environment, the ESP code has to be run in the
// Nginx thread since it may need to make remote HTTP calls.
//
// The ESP with the Nginx environment needs to implement this interface to
// jump the thread before calling ESP code.
//
// Users of CallHandler should call Check before proxying requests to
// backends, and should call Report after receiving the backend
// response.
class CallHandler {
 public:
  virtual ~CallHandler(){};

  // Checks that the supplied call is valid (i.e. should be passed
  // along to the backend) according the ESP's service configuration.
  //
  // The supplied continuation will be invoked with the backend
  // address from the service configuration (if any) and the result of
  // the check, after header adjustment (if any) takes place.  This
  // invocation may happen before Check returns.
  virtual void Check(
      std::unique_ptr<Request> request,
      std::function<void(const std::string &backend, utils::Status status)>
          continuation) = 0;

  // Reports the final outcome of a call (after the entire response
  // has been received from the backend), optionally batching multiple
  // reports for efficiency.
  //
  // The supplied continuation will be invoked after the report takes
  // place.  This invocation may happen before Report returns.
  virtual void Report(std::unique_ptr<Response> response,
                      std::function<void(void)> continuation) = 0;
};

// An ESP CallHandler that immediately and successfully completes all
// checks and issues no reports.
class NopCallHandler : public CallHandler {
 public:
  NopCallHandler() {}

  virtual ~NopCallHandler() {}

  virtual void Check(
      std::unique_ptr<Request> request,
      std::function<void(const std::string &backend, utils::Status status)>
          continuation) {
    continuation("", utils::Status::OK);
  }

  virtual void Report(std::unique_ptr<Response> response,
                      std::function<void(void)> continuation) {
    continuation();
  }
};

}  // namespace grpc
}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_GRPC_CALL_HANDLER_H_
