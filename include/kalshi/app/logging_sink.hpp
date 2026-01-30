#pragma once

#include "kalshi/logging/logger.hpp"
#include "kalshi/md/model/exchange_events.hpp"

#include <cstdint>

namespace kalshi::app
{

/**
 * Market sink that logs market data events.
 */
class LoggingSink
{
public:
  /**
   * Construct with a logger instance.
   * @param logger Logger used for event output.
   */
  explicit LoggingSink(kalshi::logging::Logger& logger) : logger_(logger) {}

  /**
   * Handle orderbook snapshots.
   * @param snapshot Snapshot payload.
   */
  void on_snapshot(const kalshi::md::OrderbookSnapshot& snapshot);

  /**
   * Handle orderbook deltas.
   * @param delta Delta payload.
   */
  void on_delta(const kalshi::md::OrderbookDelta& delta);

  /**
   * Handle trade events.
   * @param trade Trade payload.
   */
  void on_trade(const kalshi::md::TradeEvent& trade);

  /**
   * Handle market status updates.
   * @param status Status payload.
   */
  void on_status(const kalshi::md::MarketStatusUpdate& status);

private:
  kalshi::logging::Logger& logger_;
};

} // namespace kalshi::app
