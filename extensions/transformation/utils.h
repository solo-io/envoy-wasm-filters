#pragma once

#include <utility>
#include <vector>
#include <optional>
#include <string_view>


std::optional<std::string_view> getHeader(const std::vector<std::pair<std::string_view, std::string_view>>& headers,
                                   const std::string &key);