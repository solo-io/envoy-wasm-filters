#pragma once

#include <memory>
#include <unordered_map>
#include "proxy_wasm_intrinsics.h"
#include "extensions/transformation/filter.pb.h"

class StringMatcher {
public:
    virtual ~StringMatcher() = default;
    virtual bool matches(StringView s) const = 0;
};
using StringMatcherPtr = std::unique_ptr<StringMatcher>;

class HeaderMatcher {
public:
    virtual bool matches(const std::vector<std::pair<StringView, StringView>>& headers) const = 0;
    virtual ~HeaderMatcher() = default;
};
using HeaderMatcherPtr = std::unique_ptr<HeaderMatcher>;

class MatcherImpl : public HeaderMatcher {
public:
    MatcherImpl(const Matcher& );
    bool matches(const std::vector<std::pair<StringView, StringView>>& headers) const override;
private:
    std::unordered_map<std::string, StringMatcherPtr> matchers_;
};
