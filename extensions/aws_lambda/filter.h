// NOLINT(namespace-envoy)
#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>

#include "aws_authenticator.h"

//#include "absl/types/optional.h"
#include "google/protobuf/util/json_util.h"
#include "proxy_wasm_intrinsics.h"
#include "filter.pb.h"

class AwsLambdaFilterRootContext : public RootContext {
public:
  explicit AwsLambdaFilterRootContext(uint32_t id, StringView root_id) : RootContext(id, root_id) {}
  bool onConfigure(size_t /* configuration_size */) override;

  bool onStart(size_t) override;

  std::string header_value_;
};

class AwsLambdaFilterContext : public Context {
public:
  explicit AwsLambdaFilterContext(uint32_t id, RootContext* root) : Context(id, root), root_(static_cast<AwsLambdaFilterRootContext*>(static_cast<void*>(root))) {}

  void onCreate() override;
  FilterHeadersStatus onRequestHeaders(uint32_t headers) override;
  FilterDataStatus onRequestBody(size_t body_buffer_length, bool end_of_stream) override;
  FilterHeadersStatus onResponseHeaders(uint32_t headers) override;
  void onDone() override;
  void onLog() override;
  void onDelete() override;

  const std::string &path() const { return path_; }
  bool async() const { return async_; }
  const std::string &defaultBody() const { return default_body_; }
private:

  AwsLambdaFilterRootContext* root_;

  std::string path_;
  bool async_;
  //absl::optional<std::string> default_body_;
  std::string default_body_;

  unordered_map<string, string> request_headers_;
  AwsAuthenticator aws_authenticator_;

  static std::string functionUrlPath(const std::string &name,
                                     const std::string &qualifier);

  void handleDefaultBody();

  void lambdafy();
  void cleanup();
};