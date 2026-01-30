#pragma once

#include <chrono>

namespace kalshi::md {

/**
 * Websocket URL prefix.
 * @return Constant prefix string.
 */
inline constexpr std::string_view WSS_PREFIX = "wss://";
/**
 * Default connect timeout.
 * @return Timeout duration.
 */
inline constexpr std::chrono::seconds CONNECT_TIMEOUT{30};

} // namespace kalshi::md
