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
#include "src/nginx/status.h"

#include <unistd.h>
#include <fstream>

#include "google/protobuf/util/message_differencer.h"
#include "include/api_manager/api_manager.h"
#include "src/api_manager/utils/marshalling.h"
#include "src/nginx/environment.h"
#include "src/nginx/module.h"
#include "src/nginx/proto/status.pb.h"
#include "src/nginx/version.h"

extern "C" {
#include "src/core/ngx_string.h"
#include "src/http/ngx_http_request.h"
}

namespace google {
namespace api_manager {
namespace nginx {

namespace {

using std::chrono::system_clock;
using proto::ServerStatus;
using proto::ProcessStatus;
using service_control::Statistics;
using utils::Status;
using ServiceControlStatisticsProto =
    ::google::api_manager::proto::ServiceControlStatistics;
using ServiceConfigRolloutsProto =
    ::google::api_manager::proto::ServiceConfigRollouts;

#if (NGX_DARWIN)
const size_t kMemoryUnit = 1;
#else
const size_t kMemoryUnit = 1024;
#endif

ngx_str_t application_json = ngx_string("application/json");
ngx_str_t shm_name = ngx_string("esp_stats");
const std::chrono::milliseconds kRefreshInterval(1000);
const std::chrono::milliseconds kLogStatusInterval(60000);

ngx_int_t ngx_esp_stats_init_zone(ngx_shm_zone_t *shm_zone, void *data) {
  if (data) {  // nginx is being reloaded, propagate the data
    shm_zone->data = data;
    return NGX_OK;
  }

  // nginx will initialize a slab pool in shared memory but we don't need it
  shm_zone->data = shm_zone->shm.addr + sizeof(ngx_slab_pool_t);

  return NGX_OK;
}

void fill_server_status_proto(ServerStatus *server_status) {
  server_status->set_nginx_server_version(NGINX_VER_BUILD);
  server_status->set_server_version("ESP/" API_MANAGER_VERSION_STRING);
  server_status->set_built_by(NGX_COMPILER);
  server_status->set_configure(NGX_CONFIGURE);

#if (NGX_STAT_STUB)
  ServerStatus::Connections *connections = server_status->mutable_connections();
  connections->set_accepted(*ngx_stat_accepted);
  connections->set_active(*ngx_stat_active);
  connections->set_handled(*ngx_stat_handled);
  connections->set_reading(*ngx_stat_reading);
  connections->set_writing(*ngx_stat_writing);
  connections->set_waiting(*ngx_stat_waiting);

  server_status->set_requests(*ngx_stat_requests);
#endif
}

void fill_service_control_statistics(const Statistics &stat,
                                     ServiceControlStatisticsProto *pb) {
  pb->set_total_called_checks(stat.total_called_checks);
  pb->set_send_checks_by_flush(stat.send_checks_by_flush);
  pb->set_send_checks_in_flight(stat.send_checks_in_flight);
  pb->set_total_called_reports(stat.total_called_reports);
  pb->set_send_reports_by_flush(stat.send_reports_by_flush);
  pb->set_send_reports_in_flight(stat.send_reports_in_flight);
  pb->set_send_report_operations(stat.send_report_operations);
  pb->set_max_report_size(stat.max_report_size);
}

void fill_process_stats(const ngx_esp_process_stats_t &stat,
                        ProcessStatus *process_status) {
  process_status->set_process_id(stat.pid);
  process_status->mutable_started_at()->set_seconds(
      system_clock::to_time_t(stat.start_time));
  process_status->set_memory_usage(stat.current_rss);
  process_status->set_peak_memory_usage(stat.maxrss * kMemoryUnit);
  process_status->set_virtual_size(stat.virtual_size);
  process_status->set_system_cpu_time_us(stat.sys_time.count());
  process_status->set_user_cpu_time_us(stat.user_time.count());

  for (int j = 0; j < stat.num_esp; ++j) {
    auto *esp_status_proto = process_status->add_esp_status();
    esp_status_proto->set_service_name(stat.esp_stats[j].service_name);
    fill_service_control_statistics(
        stat.esp_stats[j].statistics.service_control_statistics,
        esp_status_proto->mutable_service_control_statistics());
    esp_status_proto->mutable_service_config_rollouts()->ParseFromArray(
        stat.esp_stats[j].rollouts, stat.esp_stats[j].rollouts_length);
  }
}

Status create_status_json(ngx_http_request_t *r, std::string *json) {
  nginx::proto::Status status;

  fill_server_status_proto(status.mutable_server());

  auto *ccf = reinterpret_cast<ngx_core_conf_t *>(
      ngx_get_conf(ngx_cycle->conf_ctx, ngx_core_module));

  ngx_int_t worker_processes = ccf->worker_processes;

  auto *mc = reinterpret_cast<ngx_esp_main_conf_t *>(
      ngx_http_get_module_main_conf(r, ngx_esp_module));

  auto *process_stats =
      reinterpret_cast<ngx_esp_process_stats_t *>(mc->stats_zone->data);

  for (int i = 0; i < worker_processes; ++i) {
    fill_process_stats(process_stats[i], status.add_processes());
  }

  return utils::ProtoToJson(
      status, json,
      utils::JsonOptions::PRETTY_PRINT | utils::JsonOptions::OUTPUT_DEFAULTS);
}

void get_current_memory_usage(long *virtual_size, long *current_rss) {
  // Initialize with -1 to indicate an empty value
  *virtual_size = -1;
  *current_rss = -1;
#if (NGX_LINUX)
  std::ifstream statm("/proc/self/statm");
  if (statm) {
    statm >> *virtual_size >> *current_rss;
    *virtual_size *= getpagesize();
    *current_rss *= getpagesize();
  }
#endif
}

}  // namespace

ngx_int_t ngx_esp_status_handler(ngx_http_request_t *r) {
  ngx_int_t rc;

  if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD))) {
    return NGX_HTTP_NOT_ALLOWED;
  }

  rc = ngx_http_discard_request_body(r);

  if (rc != NGX_OK) {
    return rc;
  }

  r->headers_out.content_type_len = application_json.len;
  r->headers_out.content_type = application_json;
  r->headers_out.content_type_lowcase = nullptr;

  if (r->method == NGX_HTTP_HEAD) {
    r->headers_out.status = NGX_HTTP_OK;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
      return rc;
    }
  }

  std::string status_json;
  Status status = create_status_json(r, &status_json);
  if (!status.ok()) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  };

  ngx_chain_t out;
  out.next = nullptr;

  off_t content_length = status_json.size();
  ngx_buf_t *buf = ngx_create_temp_buf(r->pool, content_length);
  if (buf == nullptr) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  buf->temporary = 1;
  buf->last_buf = (r == r->main) ? 1 : 0;
  buf->last_in_chain = 1;

  ngx_memcpy(buf->last, status_json.c_str(), content_length);
  buf->last += content_length;
  out.buf = buf;

  r->headers_out.status = NGX_HTTP_OK;
  r->headers_out.content_length_n = content_length;

  rc = ngx_http_send_header(r);
  if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
    return rc;
  }

  return ngx_http_output_filter(r, &out);
}

ngx_int_t ngx_esp_add_stats_shared_memory(ngx_conf_t *cf) {
  auto *ccf = reinterpret_cast<ngx_core_conf_t *>(
      ngx_get_conf(cf->cycle->conf_ctx, ngx_core_module));

  ngx_int_t worker_process = ccf->worker_processes;
  if (worker_process == NGX_CONF_UNSET) {
    worker_process = NGX_MAX_PROCESSES;
  }

  // nginx will initialize a slab pool in shared memory but we don't need it
  size_t shm_size = sizeof(ngx_slab_pool_t) +
                    sizeof(ngx_esp_process_stats_t) * worker_process;

  auto *shm = ngx_shared_memory_add(cf, &shm_name, shm_size, &ngx_esp_module);

  if (shm == nullptr) {
    ngx_log_error(NGX_LOG_ERR, cf->log, 0,
                  "Failed to add shared memory for stats");
    return NGX_ERROR;
  }

  shm->init = ngx_esp_stats_init_zone;

  ngx_esp_main_conf_t *mc = reinterpret_cast<ngx_esp_main_conf_t *>(
      ngx_http_conf_get_module_main_conf(cf, ngx_esp_module));

  mc->stats_zone = shm;

  return NGX_OK;
}

Status stats_json_per_process(const ngx_esp_process_stats_t &process_stats,
                              std::string *json) {
  nginx::proto::Status status;
  fill_server_status_proto(status.mutable_server());
  fill_process_stats(process_stats, status.add_processes());

  return utils::ProtoToJson(status, json, utils::JsonOptions::OUTPUT_DEFAULTS);
};

ngx_int_t ngx_esp_copy_rollouts(const ServiceConfigRolloutsProto *new_rollouts,
                                const int new_rollouts_length,
                                ngx_esp_process_stats_t *process_stat,
                                const int index) {
  char rollouts[kMaxServiceRolloutsInfoSize];
  if (new_rollouts->SerializeToArray(rollouts, new_rollouts_length)) {
    memcpy(process_stat->esp_stats[index].rollouts, rollouts,
           new_rollouts_length);
    process_stat->esp_stats[index].rollouts_length = new_rollouts_length;
  }
  return NGX_OK;
}

ngx_int_t ngx_esp_update_rollout(std::shared_ptr<ApiManager> esp,
                                 ngx_esp_process_stats_t *process_stat,
                                 int index) {
  ServiceConfigRolloutsInfo rollouts_info;
  esp->GetServiceConfigRollouts(&rollouts_info);

  ServiceConfigRolloutsProto new_rollouts;
  new_rollouts.set_rollout_id(rollouts_info.rollout_id);
  for (auto percentage : rollouts_info.percentages) {
    (*new_rollouts.mutable_percentages())[percentage.first] = percentage.second;
  }

  int length = new_rollouts.ByteSize();
  if (0 < length && length <= kMaxServiceRolloutsInfoSize) {
    ServiceConfigRolloutsProto current_rollouts;

    if (current_rollouts.ParseFromArray(
            process_stat->esp_stats[index].rollouts,
            process_stat->esp_stats[index].rollouts_length)) {
      if (!google::protobuf::util::MessageDifferencer::Equals(current_rollouts,
                                                              new_rollouts)) {
        ngx_esp_copy_rollouts(&new_rollouts, length, process_stat, index);
      }
    } else {
      ngx_esp_copy_rollouts(&new_rollouts, length, process_stat, index);
    }
  }

  return NGX_OK;
}

ngx_int_t ngx_esp_init_process_stats(ngx_cycle_t *cycle) {
  auto *mc = reinterpret_cast<ngx_esp_main_conf_t *>(
      ngx_http_cycle_get_module_main_conf(cycle, ngx_esp_module));

  if (!mc) {
    return NGX_OK;
  }

  auto *process_stats =
      reinterpret_cast<ngx_esp_process_stats_t *>(mc->stats_zone->data);

  ngx_memzero(&process_stats[ngx_worker], sizeof(ngx_esp_process_stats_t));

  auto *process_stat = &process_stats[ngx_worker];
  process_stat->pid = ngx_pid;
  process_stat->start_time = system_clock::from_time_t(ngx_time());

  ngx_esp_loc_conf_t **endpoints =
      reinterpret_cast<ngx_esp_loc_conf_t **>(mc->endpoints.elts);
  process_stat->num_esp = 0;
  int log_disabled_esp = 0;
  for (ngx_uint_t i = 0, napis = mc->endpoints.nelts; i < napis; i++) {
    ngx_esp_loc_conf_t *lc = endpoints[i];
    if (lc->esp) {
      ngx_esp_update_rollout(lc->esp, process_stat, process_stat->num_esp);

      if (lc->esp->get_logging_status_disabled()) ++log_disabled_esp;
      const std::string &service_name = lc->esp->service_name();
      // only fill buf 1 byte smaller than buffer size, always put '\0' in the
      // last byte.
      strncpy(process_stat->esp_stats[process_stat->num_esp].service_name,
              service_name.c_str(), kMaxServiceNameSize - 1);
      process_stat->esp_stats[process_stat->num_esp]
          .service_name[kMaxServiceNameSize - 1] = '\0';
      // Only report statistics for up to kMaxEspNum esp.
      if (++process_stat->num_esp >= kMaxEspNum) break;
    }
  }

  auto timer_func = [process_stat, mc]() {
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
    process_stat->user_time = std::chrono::seconds(r.ru_utime.tv_sec) +
                              std::chrono::microseconds(r.ru_utime.tv_usec);
    process_stat->sys_time = std::chrono::seconds(r.ru_stime.tv_sec) +
                             std::chrono::microseconds(r.ru_stime.tv_usec);
    process_stat->maxrss = r.ru_maxrss;
    get_current_memory_usage(&process_stat->virtual_size,
                             &process_stat->current_rss);

    int esp_idx = 0;
    ngx_esp_loc_conf_t **endpoints =
        reinterpret_cast<ngx_esp_loc_conf_t **>(mc->endpoints.elts);
    for (ngx_uint_t i = 0, napis = mc->endpoints.nelts; i < napis; i++) {
      ngx_esp_loc_conf_t *lc = endpoints[i];
      if (lc->esp) {
        ngx_esp_update_rollout(lc->esp, process_stat, esp_idx);

        lc->esp->GetStatistics(&process_stat->esp_stats[esp_idx].statistics);
        if (++esp_idx >= kMaxEspNum) break;
      }
    }
  };

  auto log_func = [cycle, process_stat]() {
    std::string status_json;
    Status status = stats_json_per_process(*process_stat, &status_json);
    if (!status.ok()) {
      return NGX_ERROR;
    };
    ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "Print endpoints status \"%s\"",
                  status_json.c_str());
    return NGX_OK;
  };

  mc->stats_timer.reset(
      new NgxEspTimer(kRefreshInterval, timer_func, cycle->log));

  // Do not log when all of esp instances disable logging.
  if (process_stat->num_esp > 0 && log_disabled_esp < process_stat->num_esp) {
    mc->log_stats_timer.reset(
        new NgxEspTimer(kLogStatusInterval, log_func, cycle->log));
  }

  return NGX_OK;
}

}  // namespace nginx
}  // namespace api_manager
}  // namespace google
