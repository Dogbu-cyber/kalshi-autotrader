#pragma once

#include <optional>
#include <string_view>

namespace kalshi::logging
{

  /** Log severity levels in ascending order. */
  enum class LogLevel
  {
    Trace,
    Debug,
    Info,
    Warn,
    Error
  };

  /**
   * Convert LogLevel to lowercase string.
   * @param level Log level to stringify.
   * @return Lowercase log level name.
   */
  [[nodiscard]] std::string_view to_string(LogLevel level);
  /**
   * Parse lowercase log level name.
   * @param text Input string.
   * @return Parsed LogLevel or std::nullopt.
   */
  [[nodiscard]] std::optional<LogLevel> parse_log_level(std::string_view text);
  /**
   * Return true if message_level should be emitted for min_level.
   * @param message_level Candidate message level.
   * @param min_level Minimum level configured.
   * @return True if the message should be emitted.
   */
  [[nodiscard]] bool should_log(LogLevel message_level, LogLevel min_level);

} // namespace kalshi::logging
