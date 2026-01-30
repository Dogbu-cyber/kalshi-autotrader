#include "kalshi/md/protocol/subscribe.hpp"

#include <cstddef>
#include <sstream>

namespace kalshi::md {

namespace {

void append_string_array(std::ostringstream &out,
                         const std::vector<std::string> &items) {
  out << "[";
  for (std::size_t i = 0; i < items.size(); ++i) {
    out << "\"" << items[i] << "\"";
    if (i + 1 < items.size()) {
      out << ",";
    }
  }
  out << "]";
}

} // namespace

std::string build_subscribe_command(const SubscribeRequest &req) {
  std::ostringstream out;
  out << "{\"id\":" << req.id << ",\"cmd\":\"subscribe\",\"params\":{";
  out << "\"channels\":";
  append_string_array(out, req.channels);
  if (!req.market_tickers.empty()) {
    out << ",\"market_tickers\":";
    append_string_array(out, req.market_tickers);
  }
  out << "}}";
  return out.str();
}

} // namespace kalshi::md
