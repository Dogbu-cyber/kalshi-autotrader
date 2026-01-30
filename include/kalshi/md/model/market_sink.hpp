#pragma once

#include <concepts>
#include <tuple>

#include "kalshi/md/model/exchange_events.hpp"

namespace kalshi::md
{

  /** Concept for compile-time market event sinks. */
  template <typename Sink>
  concept MarketSink = requires(Sink s,
                                const OrderbookSnapshot &snap,
                                const OrderbookDelta &delta,
                                const TradeEvent &trade,
                                const MarketStatusUpdate &status) {
    { s.on_snapshot(snap) } -> std::same_as<void>;
    { s.on_delta(delta) } -> std::same_as<void>;
    { s.on_trade(trade) } -> std::same_as<void>;
    { s.on_status(status) } -> std::same_as<void>;
  };

  /**
   * Fan-out sink to broadcast events to multiple sinks.
   * @tparam Sinks Sink types.
   */
  template <MarketSink... Sinks>
  class FanoutSink
  {
  public:
    /**
     * Construct a fan-out sink from references.
     * @param sinks Sink references to broadcast to.
     */
    explicit FanoutSink(Sinks &...sinks) : sinks_(sinks...) {}

    /**
     * Broadcast snapshot to sinks.
     * @param s Orderbook snapshot.
     * @return void.
     */
    void on_snapshot(const OrderbookSnapshot &s)
    {
      std::apply([&](auto &...sink)
                 { (sink.on_snapshot(s), ...); }, sinks_);
    }

    /**
     * Broadcast delta to sinks.
     * @param d Orderbook delta.
     * @return void.
     */
    void on_delta(const OrderbookDelta &d)
    {
      std::apply([&](auto &...sink)
                 { (sink.on_delta(d), ...); }, sinks_);
    }

    /**
     * Broadcast trade to sinks.
     * @param t Trade event.
     * @return void.
     */
    void on_trade(const TradeEvent &t)
    {
      std::apply([&](auto &...sink)
                 { (sink.on_trade(t), ...); }, sinks_);
    }

    /**
     * Broadcast status update to sinks.
     * @param u Market status update.
     * @return void.
     */
    void on_status(const MarketStatusUpdate &u)
    {
      std::apply([&](auto &...sink)
                 { (sink.on_status(u), ...); }, sinks_);
    }

  private:
    std::tuple<Sinks &...> sinks_;
  };

} // namespace kalshi::md
