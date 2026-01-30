#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace kalshi::md
{

  /** Market ticker identifier (e.g., KXGOVSHUT-26JAN31). */
  using MarketTicker = std::string;
  /** Monotonic sequence number per market stream. */
  using Sequence = std::uint64_t;
  /** Price in cents (0-100). */
  using Price = std::uint16_t; // (1-99 cents)
  /** Aggregate size at a price level. */
  using Size = std::uint32_t;
  /** Signed change in size for a price level. */
  using Delta = std::int32_t;
  /** Trade count (number of contracts). */
  using Count = std::uint32_t;
  /** Timestamp in nanoseconds. */
  using Timestamp = std::chrono::nanoseconds;

  /** Max supported price in cents. */
  inline constexpr Price PRICE_MAX = 100;

  /** Orderbook side. */
  enum class BookSide
  {
    Yes,
    No
  };

  /** Market lifecycle status. */
  enum class MarketStatus
  {
    Unopened,
    Open,
    Paused,
    Closed,
    Settled
  };

} // namespace kalshi::md
