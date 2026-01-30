#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace kalshi::md
{

  using MarketTicker = std::string;
  using Sequence = std::uint64_t;
  using Price = std::uint16_t; // (1-99 cents)
  using Size = std::uint32_t;
  using Delta = std::int32_t;
  using Count = std::uint32_t;
  using Timestamp = std::chrono::nanoseconds;

  inline constexpr Price PRICE_MAX = 100;

  enum class BookSide
  {
    Yes,
    No
  };

  enum class MarketStatus
  {
    Unopened,
    Open,
    Paused,
    Closed,
    Settled
  };

} // namespace kalshi::md
