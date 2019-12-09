// NOLINT(namespace-envoy)
#include <string>
#include <unordered_map>

#include "google/protobuf/util/json_util.h"
#include "proxy_wasm_intrinsics.h"
#include "extensions/transformation/filter.pb.h"
#include "extensions/transformation/transformation.h"

class TransformationRootContext : public RootContext {
public:
  explicit TransformationRootContext(uint32_t id, StringView root_id) : RootContext(id, root_id) {}
  bool onConfigure(size_t /* configuration_size */) override;

  std::string header_value_;

  std::optional<TransformationConfig> transformation_config_;
};

bool TransformationRootContext::onConfigure(size_t) {
  auto conf = getConfiguration();
  Config config;
  
  google::protobuf::util::JsonParseOptions options;
  options.case_insensitive_enum_parsing = true;
  options.ignore_unknown_fields = false;

  const auto strict_status = google::protobuf::util::JsonStringToMessage(conf->toString(), &config, options);
    if (!strict_status.ok()) {
      LOG_ERROR("failed parsing config:" + strict_status.ToString());
    // Success, no need to do any extra work.
    return false;
  }

  transformation_config_.emplace(config);
  return true; 
}

class TransformationContext : public Context {
public:
  explicit TransformationContext(uint32_t id, RootContext* root) : Context(id, root), root_(static_cast<const TransformationRootContext*>(static_cast<const void*>(root))) {}

  FilterHeadersStatus onRequestHeaders(uint32_t headers) override;
  FilterHeadersStatus onResponseHeaders(uint32_t headers) override;
private:

  std::optional<const RouteTransformers*> transformation_;
  const TransformationRootContext* root_;
};
static RegisterContextFactory register_TransformationContext(CONTEXT_FACTORY(TransformationContext),
                                                      ROOT_FACTORY(TransformationRootContext),
                                                      "transformation_root_id");

FilterHeadersStatus TransformationContext::onRequestHeaders(uint32_t) {

  auto result = getRequestHeaderPairs();
  auto pairs = result->pairs();
  
  transformation_ = root_->transformation_config_->getRouteTransformation(pairs);
  if (!transformation_) {
    return FilterHeadersStatus::Continue;
  }
  if (!transformation_.value()->request_) {
    return FilterHeadersStatus::Continue;
  }
  try {
    transformation_.value()->request_->transform(pairs, addRequestHeader, removeRequestHeader);
  } catch (std::exception& e){
    LOG_ERROR(std::string("exception while transforming request:") + e.what());
  }
  return FilterHeadersStatus::Continue;
}

FilterHeadersStatus TransformationContext::onResponseHeaders(uint32_t) {
  if (!transformation_) {
    return FilterHeadersStatus::Continue;
  }
  auto result = getResponseHeaderPairs();
  auto pairs = result->pairs();
  if (!transformation_.value()->response_) {
    return FilterHeadersStatus::Continue;
  }
  try {
   transformation_.value()->response_->transform(pairs, addResponseHeader, removeResponseHeader);
  } catch (std::exception& e){
    LOG_ERROR(std::string("exception while transforming response:") + e.what());
  }
  return FilterHeadersStatus::Continue;
}
