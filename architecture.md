# Architecture Notes

## Core idea
Use compile-time sinks for market-data dispatch. The feed handler owns the connection and parsing logic; sinks consume structured events.

## Sink contract (compile-time)
A sink must implement:
- on_snapshot(const OrderbookSnapshot&)
- on_delta(const OrderbookDelta&)
- on_trade(const TradeEvent&)
- on_status(const MarketStatusUpdate&)

This is enforced with a C++23 concept (see `include/kalshi/md/model/market_sink.hpp`).

## Fanout
`FanoutSink` can forward events to multiple sinks with zero virtual dispatch.

## Feed handler
`FeedHandler<Sink>` exposes `handle_*` methods and will own the async connection loop later. For now it is a stub (see `include/kalshi/md/feed_handler.hpp`).
