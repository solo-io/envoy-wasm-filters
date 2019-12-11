// NOLINT(namespace-envoy)
#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>

#include "extensions/aws_lambda/aws_authenticator.h"

//#include "absl/types/optional.h"
#include "google/protobuf/util/json_util.h"
#include "proxy_wasm_intrinsics.h"
#include "extensions/aws_lambda/filter.pb.h"

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
  FilterTrailersStatus onRequestTrailers(uint32_t) override;

  const std::string &path() const { return path_; }
  bool async() const { return async_; }
private:

  static const HeaderList HeadersToSign;
  
  AwsLambdaFilterRootContext* root_;

  std::string path_;
  bool async_;
  //absl::optional<std::string> default_body_;
  std::string default_body_;

  const std::vector<std::pair<std::string, std::string>>* request_headers_;
  AwsAuthenticator aws_authenticator_;

  static std::string functionUrlPath(const std::string &name,
                                     const std::string &qualifier);

  void handleDefaultBody();

  void lambdafy();
  void cleanup();
};