#pragma once

#include <concepts>
#include <tuple>

#include "kalshi/md/model/exchange_events.hpp"

namespace kalshi::md
{

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

  template <MarketSink... Sinks>
  class FanoutSink
  {
  public:
    explicit FanoutSink(Sinks &...sinks) : sinks_(sinks...) {}

    void on_snapshot(const OrderbookSnapshot &s)
    {
      std::apply([&](auto &...sink)
                 { (sink.on_snapshot(s), ...); }, sinks_);
    }

    void on_delta(const OrderbookDelta &d)
    {
      std::apply([&](auto &...sink)
                 { (sink.on_delta(d), ...); }, sinks_);
    }

    void on_trade(const TradeEvent &t)
    {
      std::apply([&](auto &...sink)
                 { (sink.on_trade(t), ...); }, sinks_);
    }

    void on_status(const MarketStatusUpdate &u)
    {
      std::apply([&](auto &...sink)
                 { (sink.on_status(u), ...); }, sinks_);
    }

  private:
    std::tuple<Sinks &...> sinks_;
  };

} // namespace kalshi::md
