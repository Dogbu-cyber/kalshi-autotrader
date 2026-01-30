#pragma once

#include <string>
#include <utility>

#include "kalshi/logging/log_event.hpp"
#include "kalshi/logging/log_level.hpp"

namespace kalshi::logging
{

  /** Sink interface for log events. */
  class Logger
  {
  public:
    virtual ~Logger() = default;

    /** Enqueue a prebuilt event. */
    virtual void log(LogEvent event) = 0;
    /** Current minimum level for emission. */
    [[nodiscard]] virtual LogLevel level() const = 0;

    /** Log a message without fields. */
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

    /** Log a message with structured fields. */
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

    /** Log a message with an attached raw payload. */
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
