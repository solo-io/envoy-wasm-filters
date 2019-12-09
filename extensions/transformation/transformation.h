#pragma once

#include<vector>
#include<optional>

#include "proxy_wasm_intrinsics.h"
#include "extensions/transformation/matcher.h"

typedef WasmResult (*RemoveHeader)(std::string_view);
typedef WasmResult (*AddHeader)(std::string_view, std::string_view);

class Transformer {
public:
  virtual ~Transformer() = default;
  virtual void transform(const std::vector<std::pair<StringView, StringView>>& headers, AddHeader add, RemoveHeader del) const = 0;
};

using TransformerConstPtr = std::unique_ptr<const Transformer>;

struct RouteTransformers {
    TransformerConstPtr request_;
    TransformerConstPtr response_;
};

class TransformationConfig {
public:
    TransformationConfig(const Config&);
    std::optional<const RouteTransformers*> getRouteTransformation(const std::vector<std::pair<StringView, StringView>>& headers) const;
private:
    std::vector<std::pair<HeaderMatcherPtr ,RouteTransformers>> transformations_;
};