#pragma once

#include <chrono>

namespace kalshi::md {

inline constexpr std::string_view WSS_PREFIX = "wss://";
inline constexpr std::chrono::seconds CONNECT_TIMEOUT{30};

} // namespace kalshi::md
