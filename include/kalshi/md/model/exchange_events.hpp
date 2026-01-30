#pragma once

#include <optional>
#include <string>
#include <vector>

#include "kalshi/md/model/types.hpp"

namespace kalshi::md {

struct PriceLevel {
  Price price; // cents
  Size size;
};

struct OrderbookSnapshot {
  MarketTicker market_ticker;
  Sequence sequence;
  std::vector<PriceLevel> yes;
  std::vector<PriceLevel> no;
  Timestamp ts;
};

struct OrderbookDelta {
  MarketTicker market_ticker;
  Sequence sequence;
  Price price;
  Delta delta;
  BookSide side;
  std::optional<std::string> client_order_id;
  Timestamp ts;
};

struct TradeEvent {
  MarketTicker market_ticker;
  Price yes_price;
  Price no_price;
  Count count;
  BookSide taker_side;
  Timestamp ts;
};

struct MarketStatusUpdate {
  MarketTicker market_ticker;
  MarketStatus status;
  Timestamp ts;
};

} // namespace kalshi::md
