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
#ifndef API_MANAGER_GCE_METADATA_H_
#define API_MANAGER_GCE_METADATA_H_

#include "include/api_manager/utils/status.h"

namespace google {
namespace api_manager {

// An environment information extracted from the Google Compute Engine metadata
// server.
class GceMetadata {
 public:
  enum STATE {
    INITIAL_STATE = 0,
    // Fetching,
    FETCHING_STATE,
    // Fetch failed
    FAILED_STATE,
    // Data is go
    DONE_STATE,
  };
  GceMetadata() : state_(INITIAL_STATE) {}

  STATE state() const { return state_; }
  void set_state(STATE state) { state_ = state; }
  bool has_valid_data() const { return state_ == DONE_STATE; }

  utils::Status ParseFromJson(std::string* json);

  const std::string& project_id() const { return project_id_; }
  const std::string& zone() const { return zone_; }
  const std::string& gae_server_software() const {
    return gae_server_software_;
  }
  const std::string& kube_env() const { return kube_env_; }

 private:
  STATE state_;
  std::string project_id_;
  std::string zone_;
  std::string gae_server_software_;
  std::string kube_env_;
};

}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_GCE_METADATA_H_
