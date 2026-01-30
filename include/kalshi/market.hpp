#pragma once

#include <cstdint>

namespace kalshi {

using MarketId = std::int32_t;

enum class MarketStatus {
  Unopened,
  Open,
  Paused,
  Closed,
  Settled
};

} // namespace kalshi
