#include "extensions/transformation/utils.h"
#include "extensions/transformation/transformation.h"
#include <regex>
#include <algorithm>

// clang-format off
#include "nlohmann/json.hpp"
// TODO: using patched inja for now; this version is the same except file operations removed
#include "extensions/transformation/inja.hpp"
// clang-format on

class Extractor {
public:
  Extractor(const Extraction &extractor);
  std::string_view extract(const std::vector<std::pair<StringView, StringView>>& headers) const;

private:
  std::string_view extractValue(std::string_view value) const;

  const std::string headername_;
  const unsigned int group_;
  const std::regex extract_regex_;
};


Extractor::Extractor(const Extraction &extractor)
    : headername_(extractor.header()), group_(extractor.subgroup()),
      extract_regex_(std::regex(extractor.regex())) {
        // mark count == number of sub groups, and we need to add one for match number 0
        // so we test for < instead of <=
        // see: http://www.cplusplus.com/reference/regex/basic_regex/mark_count/
        if (extract_regex_.mark_count() < group_) {
          throw std::logic_error("group requested for regex outside range");
        }
      }

std::string_view Extractor::extract(const std::vector<std::pair<StringView, StringView>>& headers) const {
    auto maybe_header = getHeader(headers, headername_);
    if (maybe_header.has_value()){
        return extractValue(maybe_header.value());
    }
    return "";
}

std::string_view Extractor::extractValue(std::string_view value) const {
  // get and regex
  std::match_results<std::string_view::const_iterator> regex_result;
  if (std::regex_match(value.begin(), value.end(), regex_result,
                       extract_regex_)) {
    if (group_ >= regex_result.size()) {
      // this should never happen as we test this in the ctor.
      return "";
    }
    const auto &sub_match = regex_result[group_];
    return std::string_view(sub_match.first, sub_match.length());
  }
  return "";
}
class TransformerInstance {
public:
  TransformerInstance(
      const std::vector<std::pair<StringView, StringView>>& headers,
      const std::unordered_map<std::string, std::string_view> &extractions,
      const nlohmann::json &context);
  

  std::string render(const inja::Template &input);

private:
// header_value(name)
  nlohmann::json header_callback(const inja::Arguments& args) const;
  // extracted_value(name, index)
  nlohmann::json extracted_callback(const inja::Arguments& args) const;
  
  inja::Environment env_;
  const std::vector<std::pair<StringView, StringView>>& headers_;
  const std::unordered_map<std::string, std::string_view> &extractions_;
  const nlohmann::json &context_;
};

TransformerInstance::TransformerInstance(
      const std::vector<std::pair<StringView, StringView>>& headers,
      const std::unordered_map<std::string, std::string_view> &extractions,
      const nlohmann::json &context)
    : headers_(headers), extractions_(extractions),
      context_(context) {
  env_.add_callback("header", 1,
                    [this](inja::Arguments& args) { return header_callback(args); });
  env_.add_callback("extraction", 1, [this](inja::Arguments& args) {
    return extracted_callback(args);
  });
  env_.add_callback("context", 0, [this](inja::Arguments&) { return context_; });
}

nlohmann::json TransformerInstance::header_callback(const inja::Arguments& args) const {
  const std::string& headername = args.at(0)->get_ref<const std::string&>();
  auto header_entry = getHeader(headers_, headername);
  if (!header_entry) {
    return "";
  }
  return header_entry.value();
}

nlohmann::json TransformerInstance::extracted_callback(const inja::Arguments& args) const  {
  const std::string& name = args.at(0)->get_ref<const std::string&>();
  const auto value_it = extractions_.find(name);
  if (value_it != extractions_.end()) {
    return value_it->second;
  }
  return "";
}

std::string TransformerInstance::render(const inja::Template &input) {
  // inja can't handle context that are not objects correctly, so we give it an empty object in that case
  if (context_.is_object()) {
    return env_.render(input, context_);
  } else {
    return env_.render(input, {});
  }
}


class InjaTransformer : public Transformer {
public:
  InjaTransformer(const Transformation& transformation);
  ~InjaTransformer();

  void transform(const std::vector<std::pair<StringView, StringView>>& headers, AddHeader add, RemoveHeader del) const override;

private:

  std::vector<std::pair<std::string, Extractor>> extractors_;
  std::vector<std::pair<std::string, inja::Template>> headers_;

  bool ignore_error_on_parse_;
};


InjaTransformer::InjaTransformer(const Transformation &transformation) {
  inja::ParserConfig parser_config;
  inja::LexerConfig lexer_config;
  inja::TemplateStorage template_storage;

  inja::Parser parser(parser_config, lexer_config, template_storage);

  const auto &extractors = transformation.extractors();
  for (auto it = extractors.begin(); it != extractors.end(); it++) {
    extractors_.emplace_back(std::make_pair(it->first, it->second));
  }
  const auto &headers = transformation.headers();
  for (auto it = headers.begin(); it != headers.end(); it++) {
    std::string header_name = it->first;
    std::transform(header_name.begin(), header_name.end(), header_name.begin(), tolower);
    try {
      headers_.emplace_back(std::make_pair(std::move(header_name),
                                           parser.parse(it->second.text())));
    } catch (const std::runtime_error &e) {
        // TODO: translate exception?
      throw;
    }
  }

}

InjaTransformer::~InjaTransformer() {}

void InjaTransformer::transform(const std::vector<std::pair<StringView, StringView>>& headers, AddHeader add, RemoveHeader del) const {
  nlohmann::json json_body;

  // get the extractions
  std::unordered_map<std::string, std::string_view> extractions;

  for (const auto &named_extractor : extractors_) {
    const std::string &name = named_extractor.first;
      extractions[name] = named_extractor.second.extract(headers);
  }
  // start transforming!
  TransformerInstance instance(headers, extractions, json_body);

  // Headers transform:
  for (const auto &templated_header : headers_) {
    std::string output = instance.render(templated_header.second);
    // remove existing header
    del(templated_header.first);

    if (!output.empty()) {
      // we can add the key as reference as the headers_ lifetime is as the
      // route's
      add(templated_header.first, output);
    }
  }

}

TransformationConfig::TransformationConfig(const Config& config){
    for (auto&& transformationRule : config.transformations()) {
        RouteTransformers rt;
        if (transformationRule.route_transformations().has_request_transformation()) {
            rt.request_ = std::make_unique<InjaTransformer>(transformationRule.route_transformations().request_transformation());
        }
        if (transformationRule.route_transformations().has_response_transformation()) {
            rt.response_ = std::make_unique<InjaTransformer>(transformationRule.route_transformations().response_transformation());
        }
        auto matcher = std::make_unique<MatcherImpl>(transformationRule.match());
        transformations_.emplace_back(std::make_pair(std::move(matcher), std::move(rt)));
    }

}

std::optional<const RouteTransformers*> TransformationConfig::getRouteTransformation(const std::vector<std::pair<StringView, StringView>>& headers) const {
  
  for (auto&& transformation : transformations_) {
    if (transformation.first->matches(headers)) {
      // this copy is cheap as it just copies pointers
      return std::make_optional<const RouteTransformers*>(&transformation.second);
    }
  }
  return {};
}