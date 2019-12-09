#include "extensions/transformation/utils.h"
#include <string>


// inja uses the round function which is not exported currently by envoy
// provide this until it is.
///// __attribute__ ((naked)) extern "C" double round(double d) {
///// __asm__ __volatile__("local.get 0; f64.nearest"
/////                      :
/////                      :
/////                      );
///// }

extern "C" double round(double d) {
  // TODO: findout how can we use the f64.nearest instruction here.
    return 0;
}


std::optional<std::string_view> getHeader(const std::vector<std::pair<std::string_view, std::string_view>>& headers,
                                   const std::string &key) {
    for (auto&& header : headers) {
        if (header.first == key) {
            return header.second;
        }
    }
    return {};
}