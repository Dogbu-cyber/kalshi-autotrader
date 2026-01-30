#include "kalshi/md/protocol/subscribe.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>

#include "kalshi/md/protocol/message_types.hpp"

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

bool requires_market_tickers(const std::vector<std::string> &channels) {
  return std::find(channels.begin(), channels.end(), ORDERBOOK_DELTA) !=
         channels.end();
}

} // namespace

SubscriptionCommand::SubscriptionCommand(SubscribeRequest request)
    : request_(std::move(request)),
      json_(build_subscribe_command(request_)) {}

std::expected<SubscribeRequest, SubscribeError>
build_subscribe_request(const kalshi::Config &config, int id) {
  if (requires_market_tickers(config.subscription.channels) &&
      config.subscription.market_tickers.empty()) {
    return std::unexpected(SubscribeError::MissingMarketTickers);
  }

  SubscribeRequest request{.id = id,
                           .channels = config.subscription.channels,
                           .market_tickers = config.subscription.market_tickers};
  return request;
}

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

std::expected<SubscriptionCommand, SubscribeError>
build_subscription_command(const kalshi::Config &config, int id) {
  auto request = build_subscribe_request(config, id);
  if (!request) {
    return std::unexpected(request.error());
  }
  return SubscriptionCommand(std::move(*request));
}

} // namespace kalshi::md
