#pragma once

#include <chrono>

namespace kalshi::md {

// Websocket URL prefix and default connect timeout.
inline constexpr std::string_view WSS_PREFIX = "wss://";
inline constexpr std::chrono::seconds CONNECT_TIMEOUT{30};

} // namespace kalshi::md
