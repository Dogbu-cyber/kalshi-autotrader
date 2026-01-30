#include "kalshi/app/logging_sink.hpp"

namespace kalshi::app
{

void LoggingSink::on_snapshot(const kalshi::md::OrderbookSnapshot& snapshot)
{
  kalshi::logging::LogFields fields;
  fields.add_string("market_ticker", snapshot.market_ticker);
  fields.add_uint("sequence", snapshot.sequence);
  logger_.log(kalshi::logging::LogLevel::Info, "md.sink", "orderbook_snapshot", std::move(fields));
}

void LoggingSink::on_delta(const kalshi::md::OrderbookDelta& delta)
{
  kalshi::logging::LogFields fields;
  fields.add_string("market_ticker", delta.market_ticker);
  fields.add_uint("sequence", delta.sequence);
  fields.add_uint("price", delta.price);
  fields.add_int("delta", delta.delta);
  logger_.log(kalshi::logging::LogLevel::Debug, "md.sink", "orderbook_delta", std::move(fields));
}

void LoggingSink::on_trade(const kalshi::md::TradeEvent& trade)
{
  kalshi::logging::LogFields fields;
  fields.add_string("market_ticker", trade.market_ticker);
  fields.add_uint("yes_price", trade.yes_price);
  fields.add_uint("no_price", trade.no_price);
  fields.add_uint("count", trade.count);
  logger_.log(kalshi::logging::LogLevel::Debug, "md.sink", "trade", std::move(fields));
}

void LoggingSink::on_status(const kalshi::md::MarketStatusUpdate& status)
{
  kalshi::logging::LogFields fields;
  fields.add_string("market_ticker", status.market_ticker);
  fields.add_uint("status", static_cast<std::uint64_t>(status.status));
  logger_.log(kalshi::logging::LogLevel::Info, "md.sink", "market_status", std::move(fields));
}

} // namespace kalshi::app
