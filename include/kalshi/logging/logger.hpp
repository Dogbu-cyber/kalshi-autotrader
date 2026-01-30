#pragma once

#include <string>
#include <utility>

#include "kalshi/logging/log_event.hpp"
#include "kalshi/logging/log_level.hpp"

namespace kalshi::logging
{

  class Logger
  {
  public:
    virtual ~Logger() = default;

    virtual void log(LogEvent event) = 0;
    [[nodiscard]] virtual LogLevel level() const = 0;

    void log(LogLevel level, std::string component, std::string message)
    {
      LogEvent event{.ts_ms = 0,
                     .level = level,
                     .component = std::move(component),
                     .message = std::move(message),
                     .fields = {},
                     .raw = {},
                     .include_raw = false};
      log(std::move(event));
    }

    void log(LogLevel level, std::string component, std::string message, LogFields fields)
    {
      LogEvent event{.ts_ms = 0,
                     .level = level,
                     .component = std::move(component),
                     .message = std::move(message),
                     .fields = std::move(fields),
                     .raw = {},
                     .include_raw = false};
      log(std::move(event));
    }

    void log_raw(LogLevel level,
                 std::string component,
                 std::string message,
                 LogFields fields,
                 std::string raw)
    {
      LogEvent event{.ts_ms = 0,
                     .level = level,
                     .component = std::move(component),
                     .message = std::move(message),
                     .fields = std::move(fields),
                     .raw = std::move(raw),
                     .include_raw = true};
      log(std::move(event));
    }
  };

} // namespace kalshi::logging
