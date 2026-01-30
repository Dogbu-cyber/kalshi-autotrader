#pragma once

#include <expected>
#include <string_view>

#include "kalshi/md/model/exchange_events.hpp"
#include "kalshi/md/parse/parse_errors.hpp"

namespace kalshi::md {

[[nodiscard]] std::expected<OrderbookSnapshot, ParseError> parse_orderbook_snapshot(
  std::string_view json);
[[nodiscard]] std::expected<OrderbookDelta, ParseError> parse_orderbook_delta(
  std::string_view json);
[[nodiscard]] std::expected<TradeEvent, ParseError> parse_trade_event(
  std::string_view json);

} // namespace kalshi::md
