#pragma once

#include <chrono>

namespace kalshi::md {

/** Websocket URL prefix. */
inline constexpr std::string_view WSS_PREFIX = "wss://";
/** Default connect timeout. */
inline constexpr std::chrono::seconds CONNECT_TIMEOUT{30};

} // namespace kalshi::md
