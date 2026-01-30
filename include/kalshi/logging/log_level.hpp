#pragma once

#include <optional>
#include <string_view>

namespace kalshi::logging
{

  enum class LogLevel
  {
    Trace,
    Debug,
    Info,
    Warn,
    Error
  };

  [[nodiscard]] std::string_view to_string(LogLevel level);
  [[nodiscard]] std::optional<LogLevel> parse_log_level(std::string_view text);
  [[nodiscard]] bool should_log(LogLevel message_level, LogLevel min_level);

} // namespace kalshi::logging
