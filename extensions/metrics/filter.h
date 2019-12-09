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


constexpr StringView Sep = "#@";

// The following need to be std::strings because the receiver expects a string.
const std::string unknown = "unknown";
const std::string vSource = "source";
const std::string vDest = "destination";
const std::string vDash = "-";

const std::string default_field_separator = ";.;";
const std::string default_value_separator = "=.=";
const std::string default_stat_prefix = "wasm";

#define CONFIG_DEFAULT(name) \
  config_->name().empty() ? default_##name : config_->name()

#define STD_WASM_STATS(FIELD_FUNC)     \
  FIELD_FUNC(destination_service)            \
  FIELD_FUNC(destination_port)               \
  FIELD_FUNC(request_protocol)               \
  FIELD_FUNC(response_code)                  \
  FIELD_FUNC(response_flags)                 \

struct WasmStats {
#define DEFINE_FIELD(name) std::string(name);
  STD_WASM_STATS(DEFINE_FIELD)
#undef DEFINE_FIELD

  // utility fields
  bool outbound = false;

  // Ordered dimension list is used by the metrics API.
  static std::vector<MetricTag> metricTags() {
#define DEFINE_METRIC(name) {#name, MetricTag::TagType::String},
    return std::vector<MetricTag>{STD_WASM_STATS(DEFINE_METRIC)};
#undef DEFINE_METRIC
  }

  // values is used on the datapath, only when new dimensions are found.
  std::vector<std::string> values() {
#define VALUES(name) name,
    return std::vector<std::string>{STD_WASM_STATS(VALUES)};
#undef VALUES
  }

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
  SimpleStat(uint32_t metric_id, ValueExtractorFn value_fn)
      : metric_id_(metric_id), value_fn_(value_fn){};

  inline void record(const ::Wasm::Common::RequestInfo& request_info) {
    recordMetric(metric_id_, value_fn_(request_info));
  };

  uint32_t metric_id_;

 private:
  ValueExtractorFn value_fn_;
};

// StatGen creates a SimpleStat based on resolved metric_id.
class StatGen {
 public:
  explicit StatGen(std::string name, MetricType metric_type,
                   ValueExtractorFn value_fn, std::string field_separator,
                   std::string value_separator)
      : name_(name),
        value_fn_(value_fn),
        metric_(metric_type, name, WasmStats::metricTags(),
                field_separator, value_separator){};

  StatGen() = delete;
  inline StringView name() const { return name_; };

  // Resolve metric based on provided dimension values.
  SimpleStat resolve(std::vector<std::string>& vals) {
    auto metric_id = metric_.resolveWithFields(vals);
    return SimpleStat(metric_id, value_fn_);
  };

 private:
  std::string name_;
  ValueExtractorFn value_fn_;
  Metric metric_;
};

class AddHeaderRootContext : public RootContext {
public:
  explicit AddHeaderRootContext(uint32_t id, StringView root_id) : 
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

class AddHeaderContext : public Context {
public:
  explicit AddHeaderContext(uint32_t id, RootContext* root) : 
    Context(id, root), root_(static_cast<AddHeaderRootContext*>(static_cast<void*>(root))) {}

  FilterHeadersStatus onRequestHeaders(uint32_t headers) override;
  FilterDataStatus onRequestBody(size_t body_buffer_length, bool end_of_stream) override;
  FilterDataStatus onResponseBody(size_t body_buffer_length, bool end_of_stream) override;

  void onLog() override;
private:

  AddHeaderRootContext* root_;

  Wasm::Common::RequestInfo request_info_;
};
static RegisterContextFactory register_AddHeaderContext(CONTEXT_FACTORY(AddHeaderContext),
                                                      ROOT_FACTORY(AddHeaderRootContext),
                                                      "add_header_root_id");