#pragma once

#include <expected>
#include <string_view>

#include "kalshi/md/model/exchange_events.hpp"
#include "kalshi/md/parse/parse_errors.hpp"

namespace kalshi::md
{

  /**
   * Parse full orderbook snapshot event.
   * @param json Raw websocket message.
   * @return OrderbookSnapshot or ParseError.
   */
  [[nodiscard]] std::expected<OrderbookSnapshot, ParseError> parse_orderbook_snapshot(
      std::string_view json);
  /**
   * Parse orderbook delta event.
   * @param json Raw websocket message.
   * @return OrderbookDelta or ParseError.
   */
  [[nodiscard]] std::expected<OrderbookDelta, ParseError> parse_orderbook_delta(
      std::string_view json);
  /**
   * Parse trade event.
   * @param json Raw websocket message.
   * @return TradeEvent or ParseError.
   */
  [[nodiscard]] std::expected<TradeEvent, ParseError> parse_trade_event(
      std::string_view json);

} // namespace kalshi::md
