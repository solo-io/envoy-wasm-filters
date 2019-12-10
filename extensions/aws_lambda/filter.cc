// NOLINT(namespace-envoy)
#include <string>
#include <unordered_map>

#include "aws_lambda_wasm_filter.h"

class AWSLambdaHeaderValues {
public:
  const Http::LowerCaseString InvocationType{"x-amz-invocation-type"};
  const std::string InvocationTypeEvent{"Event"};
  const std::string InvocationTypeRequestResponse{"RequestResponse"};
  const Http::LowerCaseString LogType{"x-amz-log-type"};
  const std::string LogNone{"None"};
  const Http::LowerCaseString HostHead{"x-amz-log-type"};
};

typedef ConstSingleton<AWSLambdaHeaderValues> AWSLambdaHeaderNames;

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

FilterHeadersStatus AwsLambdaFilterContext::onResponseHeaders(uint32_t) {
  LOG_DEBUG(std::string("onResponseHeaders ") + std::to_string(id()));
  addResponseHeader("newheader", root_->header_value_);
  addResponseHeader("GOAT", "TomBrady12");
  replaceResponseHeader("location", "envoy-wasm");
  return FilterHeadersStatus::Continue;
}

FilterDataStatus AwsLambdaFilterContext::onRequestBody(size_t body_buffer_length, bool end_of_stream) {
  return FilterDataStatus::Continue;
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

  handleDefaultBody();

  const std::string &invocation_type = AWSLambdaHeaderNames::get().InvocationTypeRequestResponse;
  request_headers_->addReference(AWSLambdaHeaderNames::get().InvocationType,
                                 invocation_type);
  request_headers_->addReference(AWSLambdaHeaderNames::get().LogType,
                                 AWSLambdaHeaderNames::get().LogNone);
  request_headers_->insertHost().value(protocol_options_->host());

  aws_authenticator_.sign(request_headers_, HeadersToSign,
                          protocol_options_->region());
  cleanup();
}

void AwsLambdaFilterContext::handleDefaultBody() {
  if ((!has_body_) && function_on_route_->defaultBody()) {
    Buffer::OwnedImpl data(function_on_route_->defaultBody().value());

    request_headers_->insertContentType().value().setReference(
        Http::Headers::get().ContentTypeValues.Json);
    request_headers_->insertContentLength().value(data.length());
    aws_authenticator_.updatePayloadHash(data);
    decoder_callbacks_->addDecodedData(data, false);
  }
}

void AwsLambdaFilterContext::cleanup() {
  request_headers_ = nullptr;
  function_on_route_ = nullptr;
  protocol_options_.reset();
}