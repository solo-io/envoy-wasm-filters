#include "filter.h"

const std::string requests_total = "requests_total";
const std::string request_duration_milliseconds = "request_duration_milliseconds";
const std::string request_bytes = "request_bytes";
const std::string response_bytes = "response_bytes";
const std::string extauth_rejected_requests = "extauth_rejected_requests";
const std::string ratelimited_requests = "ratelimited_requests";

bool AddHeaderRootContext::onConfigure(size_t) { 
  auto conf = getConfiguration();
  Config config;
  
  google::protobuf::util::JsonParseOptions options;
  options.case_insensitive_enum_parsing = true;
  options.ignore_unknown_fields = false;

  auto status = google::protobuf::util::JsonStringToMessage(conf->toString(), &config, options);
  if (status != Status::OK) {
    LOG_WARN("" + conf->toString());
  }
  
  outbound_ = Wasm::Common::TrafficDirection::Outbound ==
            Wasm::Common::getTrafficDirection();
  use_host_header_fallback_ = true;
  config_ = &config;

  wasm_stats_.init(outbound_);

  auto field_separator = CONFIG_DEFAULT(field_separator);
  auto value_separator = CONFIG_DEFAULT(value_separator);
  auto stat_prefix = CONFIG_DEFAULT(stat_prefix);


  stat_prefix.insert(0, "_");
  stat_prefix.append("_");

  stats_ = std::vector<StatGen>{
    StatGen(
        stat_prefix + requests_total, MetricType::Counter,
        [](const Wasm::Common::RequestInfo&) -> uint64_t { return 1; }),
    StatGen(
        stat_prefix + extauth_rejected_requests, MetricType::Counter,
        [](const Wasm::Common::RequestInfo& request_info) -> uint64_t {
          if(request_info.response_flag == "UAEX") {
            return 1;
          }
          return 0; 
        }),
    StatGen(
        stat_prefix + ratelimited_requests, MetricType::Counter,
        [](const Wasm::Common::RequestInfo& request_info) -> uint64_t {
          if(request_info.response_flag == "RL") {
            return 1;
          }
          return 0; 
        }),
    StatGen(
        stat_prefix + request_duration_milliseconds,
        MetricType::Histogram,
        [](const Wasm::Common::RequestInfo& request_info) -> uint64_t {
          return (request_info.end_timestamp - request_info.start_timestamp) /
                  1000000;
        }),
    StatGen(
        stat_prefix + request_bytes, MetricType::Histogram,
        [](const Wasm::Common::RequestInfo& request_info) -> uint64_t {
          return request_info.request_size;
        }),
    StatGen(
        stat_prefix + response_bytes, MetricType::Histogram,
        [](const Wasm::Common::RequestInfo& request_info) -> uint64_t {
          return request_info.response_size;
        })
  };
  return true; 
}

void AddHeaderRootContext::report(const Wasm::Common::RequestInfo& request_info) {
  wasm_stats_.map(request_info);
  std::vector<std::string> values;
  for (auto& statgen : stats_) {
    auto stat = statgen.resolve(values);
    stat.record(request_info);
  }
}


FilterHeadersStatus AddHeaderContext::onRequestHeaders(uint32_t) {
  request_info_.start_timestamp = getCurrentTimeNanoseconds();
  return FilterHeadersStatus::Continue;
}

FilterDataStatus AddHeaderContext::onRequestBody(size_t body_buffer_length, bool end_of_stream) {
  request_info_.request_size += body_buffer_length;
  return FilterDataStatus::Continue;
}

FilterDataStatus AddHeaderContext::onResponseBody(size_t body_buffer_length, bool end_of_stream) {
  request_info_.response_size += body_buffer_length;
  return FilterDataStatus::Continue;
}

void AddHeaderContext::onLog() { 
    Wasm::Common::populateHTTPRequestInfo(
        root_->outbound_, root_->use_host_header_fallback_, &request_info_);
    root_->report(request_info_);
 }
