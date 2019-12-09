#include "extensions/transformation/utils.h"
#include "extensions/transformation/matcher.h"

class PrefixMatcher : public StringMatcher {
public:
    PrefixMatcher(const std::string& prefix) : prefix_(prefix){}

    bool matches(StringView text) const override {
        return prefix_.empty() ||
                (text.size() >= prefix_.size() &&
                memcmp(text.data(), prefix_.data(), prefix_.size()) == 0);
    }
private:
  std::string prefix_;
};

class SuffixMatcher : public StringMatcher {
public:
    SuffixMatcher(const std::string& suffix) : suffix_(suffix){}

    bool matches(StringView text) const override {
        return suffix_.empty() ||
            (text.size() >= suffix_.size() &&
            memcmp(text.data() + (text.size() - suffix_.size()), suffix_.data(),
            suffix_.size()) == 0);
    }
private:
  std::string suffix_;
};

class ExactMatcher : public StringMatcher {
public:
    ExactMatcher(const std::string& exact) : exact_(exact){}

    bool matches(StringView text) const override {
        return exact_ == text;
    }
private:
  std::string exact_;
};

class PresentMatcher : public StringMatcher {
public:
    bool matches(StringView) const override {
        return true;
    }
};

StringMatcherPtr newMatcher(const HeaderMatch& config) {
    switch (config.header_match_specifier_case()) {
  case HeaderMatch::kExactMatch:
    return std::make_unique<ExactMatcher>(config.exact_match());
  case HeaderMatch::kPrefixMatch:
    return std::make_unique<PrefixMatcher>(config.prefix_match());
  case HeaderMatch::kSuffixMatch:
    return std::make_unique<SuffixMatcher>(config.suffix_match());
  case HeaderMatch::HEADER_MATCH_SPECIFIER_NOT_SET:
  default:
    return std::make_unique<PresentMatcher>();
  }
}

MatcherImpl::MatcherImpl(const Matcher& matcher) {
    for (const auto& header_matcher : matcher.header_matchers()) {
      matchers_[header_matcher.name()] = newMatcher(header_matcher);
    }
}

bool MatcherImpl::matches(const std::vector<std::pair<StringView, StringView>>& headers) const {

// make sure all matchers are matched
  for (auto&& pair : matchers_) {
    auto maybe_header_value = getHeader(headers, pair.first);
    if (!maybe_header_value) {
      return false;
    }

    if (!pair.second->matches(maybe_header_value.value())) {
      return false;
    }
  }


  return true;
}






