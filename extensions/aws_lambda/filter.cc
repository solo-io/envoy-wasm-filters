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
  
  static constexpr std::string_view Host{"host"};
  static constexpr std::string_view ContentType{"content-type"};
};

const HeaderList AwsLambdaFilterContext::HeadersToSign =
    AwsAuthenticator::createHeaderToSign(
        {std::string(AWSLambdaHeaderValues::InvocationType),
         std::string(AWSLambdaHeaderValues::LogType), std::string(AWSLambdaHeaderValues::Host),
         std::string(AWSLambdaHeaderValues::ContentType)});

static RegisterContextFactory register_AwsLambdaFilterContext(CONTEXT_FACTORY(AwsLambdaFilterContext),
                                                      ROOT_FACTORY(AwsLambdaFilterRootContext),
                                                      "aws_lambda_root_id");

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
  // heuristic till end_stream is exposed in envoy-wasm
  auto method = getRequestHeader(":method");
  bool end_stream = (method->view() == "GET");
  LOG_DEBUG(std::string("onRequestHeaders ") + std::to_string(id()));
  if (end_stream) {
    lambdafy();
    return FilterHeadersStatus::Continue;
  }
  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus AwsLambdaFilterContext::onRequestBody(size_t body_buffer_length, bool end_stream) {
  if (end_stream) {
    auto body = getBufferBytes(BufferType::HttpRequestBody, 0, body_buffer_length);
    aws_authenticator_.updatePayloadHash(body->view());
    lambdafy();
    return FilterDataStatus::Continue;
  }
  return FilterDataStatus::StopIterationAndBuffer;
}

FilterTrailersStatus AwsLambdaFilterContext::onRequestTrailers(uint32_t) {
  lambdafy();
  return FilterTrailersStatus::Continue;
}


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
  aws_authenticator_.sign(*request_headers_, HeadersToSign ,
                          "us-east-1"); // TODO 
}
