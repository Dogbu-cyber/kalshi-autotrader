#pragma once

namespace kalshi::md
{

  /** Message type strings from Kalshi websocket API. */
  inline constexpr const char *ORDERBOOK_SNAPSHOT = "orderbook_snapshot";
  inline constexpr const char *ORDERBOOK_DELTA = "orderbook_delta";
  inline constexpr const char *TRADE = "trade";
  inline constexpr const char *TICKER = "ticker";
  inline constexpr const char *MARKET_STATUS = "market_status";
  inline constexpr const char *SUBSCRIBED = "subscribed";
  inline constexpr const char *ERROR = "error";

} // namespace kalshi::md
