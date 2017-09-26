#include "src/nginx/grpc_web_server_call.h"

#include "src/nginx/grpc_web_finish.h"

namespace google {
namespace api_manager {
namespace nginx {
namespace {
const ngx_str_t kContentTypeGrpcWeb = ngx_string("application/grpc-web");
}  // namespace

utils::Status NgxEspGrpcWebServerCall::Create(
    ngx_http_request_t* r, std::shared_ptr<NgxEspGrpcWebServerCall>* out) {
  std::shared_ptr<NgxEspGrpcWebServerCall> call(new NgxEspGrpcWebServerCall(r));
  auto status = call->ProcessPrereadRequestBody();
  if (!status.ok()) {
    return status;
  }
  *out = call;
  return utils::Status::OK;
}

NgxEspGrpcWebServerCall::NgxEspGrpcWebServerCall(ngx_http_request_t* r)
    : NgxEspGrpcPassThroughServerCall(r) {}

NgxEspGrpcWebServerCall::~NgxEspGrpcWebServerCall() {}

void NgxEspGrpcWebServerCall::Finish(
    const utils::Status& status,
    std::multimap<std::string, std::string> response_trailers) {
  if (!cln_.data) {
    return;
  }

  // Make sure the headers have been sent
  if (!r_->header_sent) {
    auto status = WriteDownstreamHeaders();
    if (!status.ok()) {
      ngx_http_finalize_request(r_,
                                GrpcWebFinish(r_, status, response_trailers));
      return;
    }
  }

  ngx_http_finalize_request(r_, GrpcWebFinish(r_, status, response_trailers));
}

const ngx_str_t& NgxEspGrpcWebServerCall::response_content_type() const {
  return kContentTypeGrpcWeb;
}
}  // namespace nginx
}  // namespace api_manager
}  // namespace google
