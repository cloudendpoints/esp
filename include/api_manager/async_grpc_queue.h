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
#ifndef API_MANAGER_ASYNC_GRPC_QUEUE_H_
#define API_MANAGER_ASYNC_GRPC_QUEUE_H_

#include <grpc++/grpc++.h>

namespace google {
namespace api_manager {

// An interface to an asynchronous GRPC queue, wrapping tag-creation
// functionality with a GRPC queue backed by an executor that will run
// the callbacks used to create the tags.
class AsyncGrpcQueue {
 public:
  virtual ~AsyncGrpcQueue() {}

  // Makes a tag to describe the continuation of an asynchronous
  // operation invoked using the GRPC completion queue returned by
  // GetQueue().  Each tag is valid for exactly one asynchronous call,
  // and must be used for a call (or the tag's memory will be leaked).
  virtual void *MakeTag(std::function<void(bool)> callback) = 0;

  // This method returns a GRPC completion queue.  All tags queued to
  // the completion queue must be created by calling MakeTag().
  virtual ::grpc::CompletionQueue *GetQueue() = 0;
};

}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_ASYNC_GRPC_QUEUE_H_
