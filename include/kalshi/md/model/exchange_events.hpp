#pragma once

#include <optional>
#include <string>
#include <vector>

#include "kalshi/md/model/types.hpp"

namespace kalshi::md
{

  /** Single price level in an orderbook. */
  struct PriceLevel
  {
    Price price; // cents
    Size size;
  };

  /** Full orderbook snapshot. */
  struct OrderbookSnapshot
  {
    MarketTicker market_ticker;
    Sequence sequence;
    std::vector<PriceLevel> yes;
    std::vector<PriceLevel> no;
    Timestamp ts;
  };

  /** Incremental orderbook update. */
  struct OrderbookDelta
  {
    MarketTicker market_ticker;
    Sequence sequence;
    Price price;
    Delta delta;
    BookSide side;
    std::optional<std::string> client_order_id;
    Timestamp ts;
  };

  /** Trade execution event. */
  struct TradeEvent
  {
    MarketTicker market_ticker;
    Price yes_price;
    Price no_price;
    Count count;
    BookSide taker_side;
    Timestamp ts;
  };

  /** Market status update event. */
  struct MarketStatusUpdate
  {
    MarketTicker market_ticker;
    MarketStatus status;
    Timestamp ts;
  };

} // namespace kalshi::md
