// NOLINT(namespace-envoy)
#include <string>
#include <unordered_map>

#include "extensions/aws_lambda/filter.h"

class AWSLambdaHeaderValues {
public:
  static constexpr std::string_view InvocationType{"x-amz-invocation-type"};
  static constexpr std::string_view InvocationTypeEvent{"Event"};
  static constexpr std::string_view InvocationTypeRequestResponse{"RequestResponse"};
  static constexpr std::string_view LogType{"x-amz-log-type"};
  static constexpr std::string_view LogNone{"None"};
  static constexpr std::string_view HostHead{"x-amz-log-type"};
};

static RegisterContextFactory register_AwsLambdaFilterContext(CONTEXT_FACTORY(AwsLambdaFilterContext),
                                                      ROOT_FACTORY(AwsLambdaFilterRootContext),
                                                      "add_header_root_id");

bool AwsLambdaFilterRootContext::onConfigure(size_t) {
  auto conf = getConfiguration();
  Config config;

  google::protobuf::util::JsonParseOptions options;
  options.case_insensitive_enum_parsing = true;
  options.ignore_unknown_fields = false;

  google::protobuf::util::JsonStringToMessage(conf->toString(), &config, options);
  LOG_DEBUG("onConfigure " + config.value());
  header_value_ = config.value();
  return true;
}

bool AwsLambdaFilterRootContext::onStart(size_t) { LOG_DEBUG("onStart"); return true;}

void AwsLambdaFilterContext::onCreate() { LOG_DEBUG(std::string("onCreate " + std::to_string(id()))); }

FilterHeadersStatus AwsLambdaFilterContext::onRequestHeaders(uint32_t) {
  LOG_DEBUG(std::string("onRequestHeaders ") + std::to_string(id()));
  lambdafy();
  return FilterHeadersStatus::Continue;
}

FilterDataStatus AwsLambdaFilterContext::onRequestBody(size_t body_buffer_length, bool end_of_stream) {
  return FilterDataStatus::Continue;
}

FilterTrailersStatus AwsLambdaFilterContext::onRequestTrailers(uint32_t) {
  LOG_DEBUG(std::string("onRequestHeaders ") + std::to_string(id()));
  lambdafy();
  return FilterTrailersStatus::Continue;
}


void AwsLambdaFilterContext::onDone() { LOG_DEBUG(std::string("onDone " + std::to_string(id()))); }

void AwsLambdaFilterContext::onLog() { LOG_DEBUG(std::string("onLog " + std::to_string(id()))); }

void AwsLambdaFilterContext::onDelete() { LOG_DEBUG(std::string("onDelete " + std::to_string(id()))); }

std::string
AwsLambdaFilterContext::functionUrlPath(const std::string &name,
                                      const std::string &qualifier) {

  std::stringstream val;
  val << "/2015-03-31/functions/" << name << "/invocations";
  if (!qualifier.empty()) {
    val << "?Qualifier=" << qualifier;
  }
  return val.str();
}

void AwsLambdaFilterContext::lambdafy() {

  std::string_view invocation_type = AWSLambdaHeaderValues::InvocationTypeRequestResponse;
  addRequestHeader(AWSLambdaHeaderValues::InvocationType,
                                 invocation_type);
  addRequestHeader(AWSLambdaHeaderValues::LogType,
                                 AWSLambdaHeaderValues::LogNone);
  // replaceRequestHeader(":authority", protocol_options_->host());
HeaderList HeadersToSign; // TODO
  aws_authenticator_.sign(*request_headers_, HeadersToSign ,
                          "us-east-1"); // TODO
  cleanup();
}

void AwsLambdaFilterContext::cleanup() {
  request_headers_ = nullptr;
  // protocol_options_.reset();
}