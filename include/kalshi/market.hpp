#pragma once

#include <cstdint>

namespace kalshi
{

  /** Market identifier. */
  using MarketId = std::int32_t;

  /** High-level market status values. */
  enum class MarketStatus
  {
    Unopened,
    Open,
    Paused,
    Closed,
    Settled
  };

} // namespace kalshi
