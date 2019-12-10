// NOLINT(namespace-envoy)
#include <string>
#include <sstream>
#include <unordered_map>

#include "google/protobuf/util/json_util.h"
#include "extensions/metrics/filter.pb.h"
#include "extensions/common/context.h"
#include "proxy_wasm_intrinsics.h"


using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::Status;

const std::string default_stat_prefix = "wasm";

#define CONFIG_DEFAULT(name) \
  config_->name().empty() ? default_##name : config_->name()

#define STATS_DIMENSIONS(FIELD_FUNC)     \
  FIELD_FUNC(destination_service)            \
  FIELD_FUNC(destination_port)               \
  FIELD_FUNC(request_protocol)               \
  FIELD_FUNC(response_code)                  \
  FIELD_FUNC(response_flags)                 \


struct WasmStats {

#define DEFINE_FIELD(name) std::string(name);
  STATS_DIMENSIONS(DEFINE_FIELD)
#undef DEFINE_FIELD

  // utility fields
  bool outbound = false;

  // maps from request context to dimensions.
  // local node derived dimensions are already filled in.
  void map_request(const ::Wasm::Common::RequestInfo& request) {
    destination_service = request.destination_service_host;
    destination_port = std::to_string(request.destination_port);

    request_protocol = request.request_protocol;
    response_code = std::to_string(request.response_code);
    response_flags = request.response_flag;
  }

 public:
  // Called during intialization.
  // initialize properties that do not vary by requests.
  // Properties are different based on inbound / outbound.
  void init(bool out_bound) {
    outbound = out_bound;
  }

    // maps peer_node and request to dimensions.
  void map(const ::Wasm::Common::RequestInfo& request) {
    map_request(request);
  }
};


using ValueExtractorFn =
    uint64_t (*)(const ::Wasm::Common::RequestInfo& request_info);

// SimpleStat record a pre-resolved metric based on the values function.
class SimpleStat {
 public:
  SimpleStat(uint32_t metric_id, MetricType metric_type, ValueExtractorFn value_fn)
      : metric_id_(metric_id), metric_type_(metric_type), value_fn_(value_fn){};

  inline void record(const ::Wasm::Common::RequestInfo& request_info) {
    switch (metric_type_) {
      case MetricType::Counter:
        incrementMetric(metric_id_, value_fn_(request_info));
      default:
        recordMetric(metric_id_, value_fn_(request_info));
    }
  };

  uint32_t metric_id_;

  MetricType metric_type_;

 private:
  ValueExtractorFn value_fn_;
};

// StatGen creates a SimpleStat based on resolved metric_id.
class StatGen {
 public:
  explicit StatGen(std::string name, MetricType metric_type,
                   ValueExtractorFn value_fn)
      : name_(name),
        value_fn_(value_fn),
        metric_(metric_type, name){};

  StatGen() = delete;
  inline StringView name() const { return name_; };

  // Resolve metric based on provided dimension values.
  SimpleStat resolve(std::vector<std::string>& vals) {
    auto metric_id = metric_.resolveWithFields(vals);
    return SimpleStat(metric_id, metric_.type, value_fn_);
  };

 private:
  std::string name_;
  ValueExtractorFn value_fn_;
  Metric metric_;
};

class StatsRootContext : public RootContext {
public:
  explicit StatsRootContext(uint32_t id, StringView root_id) : 
    RootContext(id, root_id), config_(nullptr)  {}
  bool onConfigure(size_t /* configuration_size */) override;

  void report(const ::Wasm::Common::RequestInfo& request_info);

  bool outbound_;
  bool use_host_header_fallback_;

private:
  WasmStats wasm_stats_;
  Config* config_;
  std::vector<StatGen> stats_;
};

class StatsContext : public Context {
public:
  explicit StatsContext(uint32_t id, RootContext* root) : 
    Context(id, root), root_(static_cast<StatsRootContext*>(static_cast<void*>(root))) {}

  FilterHeadersStatus onRequestHeaders(uint32_t headers) override;
  FilterDataStatus onRequestBody(size_t body_buffer_length, bool end_of_stream) override;
  FilterDataStatus onResponseBody(size_t body_buffer_length, bool end_of_stream) override;

  void onLog() override;
private:

  StatsRootContext* root_;

  Wasm::Common::RequestInfo request_info_;
};
static RegisterContextFactory register_StatsContext(CONTEXT_FACTORY(StatsContext),
                                                      ROOT_FACTORY(StatsRootContext),
                                                      "stats_root_id");