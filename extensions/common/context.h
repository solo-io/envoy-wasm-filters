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

#pragma once

#include <set>

#include "proxy_wasm_intrinsics.h"

namespace Wasm {
namespace Common {

// Header keys
constexpr StringView kAuthorityHeaderKey = ":authority";
constexpr StringView kMethodHeaderKey = ":method";
constexpr StringView kContentTypeHeaderKey = "content-type";

const std::string kProtocolHTTP = "http";
const std::string kProtocolGRPC = "grpc";

const std::set<std::string> kGrpcContentTypes{
    "application/grpc", "application/grpc+proto", "application/grpc+json"};

enum class ServiceAuthenticationPolicy : int64_t {
  Unspecified = 0,
  None = 1,
  MutualTLS = 2,
};

constexpr StringView kMutualTLS = "MUTUAL_TLS";
constexpr StringView kNone = "NONE";

StringView AuthenticationPolicyString(ServiceAuthenticationPolicy policy);

// RequestInfo represents the information collected from filter stream
// callbacks. This is used to fill metrics and logs.
struct RequestInfo {
  // Start timestamp in nanoseconds.
  int64_t start_timestamp = 0;

  // End timestamp in nanoseconds.
  int64_t end_timestamp = 0;

  // Request total size in bytes, include header, body, and trailer.
  int64_t request_size = 0;

  // Response total size in bytes, include header, body, and trailer.
  int64_t response_size = 0;

  // Destination port that the request targets.
  uint32_t destination_port = 0;

  // Protocol used the request (HTTP/1.1, gRPC, etc).
  std::string request_protocol;

  // Response code of the request.
  uint32_t response_code = 0;

  // Response flag giving additional information - NR, UAEX etc.
  // TODO populate
  std::string response_flag;

  // Host name of destination service.
  std::string destination_service_host;

  // Short name of destination service.
  std::string destination_service_name;

  // Operation of the request, i.e. HTTP method or gRPC API method.
  std::string request_operation;

  // The path portion of the URL without the query string.
  std::string request_url_path;
};

// RequestContext contains all the information available in the request.
// Some or all part may be populated depending on need.
struct RequestContext {
  const bool outbound;
  const Common::RequestInfo& request;
};

// TrafficDirection is a mirror of envoy xDS traffic direction.
enum class TrafficDirection : int64_t {
  Unspecified = 0,
  Inbound = 1,
  Outbound = 2,
};

// Retrieves the traffic direction from the configuration context.
TrafficDirection getTrafficDirection();


// populateHTTPRequestInfo populates the RequestInfo struct. It needs access to
// the request context.
void populateHTTPRequestInfo(bool outbound, bool use_host_header,
                             RequestInfo* request_info);

}  // namespace Common
}  // namespace Wasm