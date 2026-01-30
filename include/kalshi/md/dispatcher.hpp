#pragma once

#include <expected>
#include <string_view>

#include "kalshi/md/model/market_sink.hpp"
#include "kalshi/md/parse/event_parser.hpp"
#include "kalshi/md/parse/message_parser.hpp"
#include "kalshi/md/parse/parse_errors.hpp"
#include "kalshi/md/protocol/message_types.hpp"

namespace kalshi::md
{

  // Dispatches parsed websocket messages to a market sink.
  template <MarketSink Sink>
  class Dispatcher
  {
  public:
    explicit Dispatcher(Sink &sink) : sink_(sink) {}

    // Parse type and route to the appropriate sink handler.
    [[nodiscard]] std::expected<void, ParseError> on_message(std::string_view json)
    {
      auto type = parse_message_type(json);
      if (!type)
      {
        return std::unexpected(type.error());
      }

      if (*type == ORDERBOOK_SNAPSHOT)
      {
        auto snapshot = parse_orderbook_snapshot(json);
        if (!snapshot)
        {
          return std::unexpected(snapshot.error());
        }
        sink_.on_snapshot(*snapshot);
        return {};
      }

      if (*type == ORDERBOOK_DELTA)
      {
        auto delta = parse_orderbook_delta(json);
        if (!delta)
        {
          return std::unexpected(delta.error());
        }
        sink_.on_delta(*delta);
        return {};
      }

      if (*type == TRADE)
      {
        auto trade = parse_trade_event(json);
        if (!trade)
        {
          return std::unexpected(trade.error());
        }
        sink_.on_trade(*trade);
        return {};
      }

      return std::unexpected(ParseError::UnsupportedType);
    }

  private:
    Sink &sink_;
  };

} // namespace kalshi::md
