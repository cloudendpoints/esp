// Copyright (C) Extensible Service Proxy Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "src/grpc/transcoding/transcoder_factory.h"

#include <memory>
#include <string>
#include <vector>

#include "google/api/service.pb.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/stubs/common.h"
#include "google/protobuf/stubs/status.h"
#include "google/protobuf/util/json_util.h"
#include "grpc_transcoding/json_request_translator.h"
#include "grpc_transcoding/message_stream.h"
#include "grpc_transcoding/response_to_json_translator.h"
#include "grpc_transcoding/type_helper.h"
#include "include/api_manager/method_call_info.h"
#include "src/api_manager/utils/marshalling.h"

namespace google {
namespace api_manager {
namespace transcoding {
namespace {

namespace pb = ::google::protobuf;
namespace pbio = ::google::protobuf::io;
namespace pbutil = ::google::protobuf::util;
namespace pberr = ::google::protobuf::util::error;

using ::google::grpc::transcoding::JsonRequestTranslator;
using ::google::grpc::transcoding::RequestInfo;
using ::google::grpc::transcoding::RequestWeaver;
using ::google::grpc::transcoding::ResponseToJsonTranslator;
using ::google::grpc::transcoding::Transcoder;
using ::google::grpc::transcoding::TranscoderInputStream;
using ::google::grpc::transcoding::TypeHelper;

// Transcoder implementation based on JsonRequestTranslator &
// ResponseToJsonTranslator
class TranscoderImpl : public Transcoder {
 public:
  // request_translator - a JsonRequestTranslator that does the request
  //                      translation
  // response_translator - a ResponseToJsonTranslator that does the response
  //                       translation
  TranscoderImpl(std::unique_ptr<JsonRequestTranslator> request_translator,
                 std::unique_ptr<ResponseToJsonTranslator> response_translator)
      : request_translator_(std::move(request_translator)),
        response_translator_(std::move(response_translator)),
        request_stream_(request_translator_->Output().CreateInputStream()),
        response_stream_(response_translator_->CreateInputStream()) {}

  // Transcoder implementation
  TranscoderInputStream* RequestOutput() { return request_stream_.get(); }
  pbutil::Status RequestStatus() {
    return request_translator_->Output().Status();
  }

  pbio::ZeroCopyInputStream* ResponseOutput() { return response_stream_.get(); }
  pbutil::Status ResponseStatus() { return response_translator_->Status(); }

 private:
  std::unique_ptr<JsonRequestTranslator> request_translator_;
  std::unique_ptr<ResponseToJsonTranslator> response_translator_;
  std::unique_ptr<TranscoderInputStream> request_stream_;
  std::unique_ptr<TranscoderInputStream> response_stream_;
};

// Converts MethodCallInfo into a RequestInfo structure needed by the
// JsonRequestTranslator.
pbutil::Status MethodCallInfoToRequestInfo(TypeHelper* type_helper,
                                           const MethodCallInfo& call_info,
                                           RequestInfo* request_info) {
  // Try to resolve the request type
  const auto& request_type_url = call_info.method_info->request_type_url();
  request_info->message_type =
      type_helper->Info()->GetTypeByTypeUrl(request_type_url);
  if (nullptr == request_info->message_type) {
    return pbutil::Status(pberr::NOT_FOUND,
                          "Could not resolve the type \"" + request_type_url +
                              "\". Invalid service configuration.");
  }

  // Copy the body field path
  request_info->body_field_path = call_info.body_field_path;

  // Resolve the field paths of the bindings and add to the request_info
  for (const auto& unresolved_binding : call_info.variable_bindings) {
    RequestWeaver::BindingInfo resolved_binding;

    // Verify that the value is valid UTF8 before continuing
    if (!pb::internal::IsStructurallyValidUTF8(
            unresolved_binding.value.c_str(),
            unresolved_binding.value.size())) {
      return pbutil::Status(pberr::INVALID_ARGUMENT,
                            "Encountered non UTF-8 code points.");
    }

    resolved_binding.value = unresolved_binding.value;

    // Try to resolve the field path
    auto status = type_helper->ResolveFieldPath(*request_info->message_type,
                                                unresolved_binding.field_path,
                                                &resolved_binding.field_path);
    if (!status.ok()) {
      // Field path could not be resolved (usually a config error) - return
      // the error.
      return status;
    }

    request_info->variable_bindings.emplace_back(std::move(resolved_binding));
  }

  return pbutil::Status::OK;
}

// This class combines two resolvers: if the first one could not find it,
// try the second.
class TwoTypeResolvers : public pbutil::TypeResolver {
 public:
  TwoTypeResolvers(pbutil::TypeResolver* resolver1,
                   pbutil::TypeResolver* resolver2)
      : resolver1_(resolver1), resolver2_(resolver2) {}

  pbutil::Status ResolveMessageType(const std::string& type_url,
                                    google::protobuf::Type* type) override {
    auto status = resolver1_->ResolveMessageType(type_url, type);
    if (!status.ok()) {
      return resolver2_->ResolveMessageType(type_url, type);
    }
    return status;
  }

  pbutil::Status ResolveEnumType(const std::string& type_url,
                                 google::protobuf::Enum* enum_type) override {
    auto status = resolver1_->ResolveEnumType(type_url, enum_type);
    if (!status.ok()) {
      return resolver2_->ResolveEnumType(type_url, enum_type);
    }
    return status;
  }

 private:
  pbutil::TypeResolver* resolver1_;
  pbutil::TypeResolver* resolver2_;
};

}  // namespace

TranscoderFactory::TranscoderFactory(
    const ::google::api::Service& service,
    const ::google::protobuf::util::JsonPrintOptions& json_print_options)
    : type_helper_(service.types(), service.enums()),
      json_print_options_(json_print_options),
      status_resolver_(new TwoTypeResolvers(type_helper_.Resolver(),
                                            utils::GetTypeResolver())) {}

pbutil::Status TranscoderFactory::Create(
    const MethodCallInfo& call_info, pbio::ZeroCopyInputStream* request_input,
    TranscoderInputStream* response_input,
    std::unique_ptr<Transcoder>* transcoder) {
  // Convert MethodCallInfo into RequestInfo
  RequestInfo request_info;
  auto status =
      MethodCallInfoToRequestInfo(&type_helper_, call_info, &request_info);
  if (!status.ok()) {
    return status;
  }

  // For now we support only HTTP/JSON <=> gRPC transcoding.

  // Create a JsonRequestTranslator for translating the request
  std::unique_ptr<JsonRequestTranslator> request_translator(
      new JsonRequestTranslator(type_helper_.Resolver(), request_input,
                                request_info,
                                call_info.method_info->request_streaming(),
                                /*output_delimiters*/ true));

  // Create a ResponseToJsonTranslator for translating the response
  std::unique_ptr<ResponseToJsonTranslator> response_translator(
      new ResponseToJsonTranslator(type_helper_.Resolver(),
                                   call_info.method_info->response_type_url(),
                                   call_info.method_info->response_streaming(),
                                   response_input, json_print_options_));

  // Create the Transcoder
  transcoder->reset(new TranscoderImpl(std::move(request_translator),
                                       std::move(response_translator)));

  return pbutil::Status::OK;
}

}  // namespace transcoding
}  // namespace api_manager
}  // namespace google
