  
/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include<vector>
#include<sstream>

#include "extensions/common/context.h"

#include "extensions/common/util.h"
#include "google/protobuf/util/json_util.h"

namespace Wasm {
namespace Common {

const char kRbacFilterName[] = "envoy.filters.http.rbac";
const char kRbacPermissivePolicyIDField[] = "shadow_effective_policy_id";
const char kRbacPermissiveEngineResultField[] = "shadow_engine_result";

namespace {


// Extract service name from service fqdn.
void extractServiceName(const std::string& fqdn, std::string* service_name) {
  std::stringstream ss(fqdn);
  std::vector<std::string> result;
  while( ss.good() )
  {
    std::string substr;
    std::getline( ss, substr, ',' );
    result.push_back( substr );
  }
  if (result.size() > 0) {
    *service_name = result[0];
  }
}

}  // namespace

StringView AuthenticationPolicyString(ServiceAuthenticationPolicy policy) {
  switch (policy) {
    case ServiceAuthenticationPolicy::None:
      return kNone;
    case ServiceAuthenticationPolicy::MutualTLS:
      return kMutualTLS;
    default:
      break;
  }
  return {};
  ;
}

// Retrieves the traffic direction from the configuration context.
TrafficDirection getTrafficDirection() {
  int64_t direction;
  if (getValue({"listener_direction"}, &direction)) {
    return static_cast<TrafficDirection>(direction);
  }
  return TrafficDirection::Unspecified;
}

using google::protobuf::util::JsonStringToMessage;
using google::protobuf::util::MessageToJsonString;


// Host header is used if use_host_header_fallback==true.
// Normally it is ok to use host header within the mesh, but not at ingress.
void populateHTTPRequestInfo(bool outbound, bool use_host_header_fallback,
                             RequestInfo* request_info) {
  // TODO: switch to stream_info.requestComplete() to avoid extra compute.
  request_info->end_timestamp = getCurrentTimeNanoseconds();

  // Fill in request info.
  int64_t response_code = 0;
  if (getValue({"response", "code"}, &response_code)) {
    request_info->response_code = response_code;
  }

  if (kGrpcContentTypes.count(getHeaderMapValue(HeaderMapType::RequestHeaders,
                                                kContentTypeHeaderKey)
                                  ->toString()) != 0) {
    request_info->request_protocol = kProtocolGRPC;
  } else {
    // TODO Add http/1.1, http/1.0, http/2 in a separate attribute.
    // http|grpc classification is compatible with Mixerclient
    request_info->request_protocol = kProtocolHTTP;
  }

  // Try to get fqdn of destination service from cluster name. If not found, use
  // host header instead.
  std::string cluster_name = "";
  getStringValue({"cluster_name"}, &cluster_name);
  // extractFqdn(cluster_name, &request_info->destination_service_host);
  if (!request_info->destination_service_host.empty()) {
    // cluster name follows Istio convention, so extract out service name.
    extractServiceName(request_info->destination_service_host,
                       &request_info->destination_service_name);
  } else if (use_host_header_fallback) {
    // fallback to host header if requested.
    request_info->destination_service_host =
        getHeaderMapValue(HeaderMapType::RequestHeaders, kAuthorityHeaderKey)
            ->toString();
    // TODO: what is the proper fallback for destination service name?
  }

  request_info->request_operation =
      getHeaderMapValue(HeaderMapType::RequestHeaders, kMethodHeaderKey)
          ->toString();

  getStringValue({"request", "url_path"}, &request_info->request_url_path);

  int64_t destination_port = 0;

  request_info->destination_port = destination_port;

  uint64_t response_flags = 0;
  getValue({"response", "flags"}, &response_flags);
  request_info->response_flag = parseResponseFlag(response_flags);
}

}  // namespace Common
}  // namespace Wasm