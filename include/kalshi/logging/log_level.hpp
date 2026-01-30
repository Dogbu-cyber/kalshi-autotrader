#pragma once

#include <optional>
#include <string_view>

namespace kalshi::logging
{

  // Log severity levels in ascending order.
  enum class LogLevel
  {
    Trace,
    Debug,
    Info,
    Warn,
    Error
  };

  // Convert LogLevel to lowercase string.
  [[nodiscard]] std::string_view to_string(LogLevel level);
  // Parse lowercase log level name.
  [[nodiscard]] std::optional<LogLevel> parse_log_level(std::string_view text);
  // Returns true if message_level should be emitted for min_level.
  [[nodiscard]] bool should_log(LogLevel message_level, LogLevel min_level);

} // namespace kalshi::logging
