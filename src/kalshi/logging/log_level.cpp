#include "kalshi/logging/log_level.hpp"

namespace kalshi::logging
{

  std::string_view to_string(LogLevel level)
  {
    switch (level)
    {
    case LogLevel::Trace:
      return "trace";
    case LogLevel::Debug:
      return "debug";
    case LogLevel::Info:
      return "info";
    case LogLevel::Warn:
      return "warn";
    case LogLevel::Error:
      return "error";
    }
    return "info";
  }

  std::optional<LogLevel> parse_log_level(std::string_view text)
  {
    if (text == "trace")
    {
      return LogLevel::Trace;
    }
    if (text == "debug")
    {
      return LogLevel::Debug;
    }
    if (text == "info")
    {
      return LogLevel::Info;
    }
    if (text == "warn")
    {
      return LogLevel::Warn;
    }
    if (text == "error")
    {
      return LogLevel::Error;
    }
    return std::nullopt;
  }

  bool should_log(LogLevel message_level, LogLevel min_level)
  {
    return static_cast<int>(message_level) >= static_cast<int>(min_level);
  }

} // namespace kalshi::logging
