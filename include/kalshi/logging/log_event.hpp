#pragma once

#include <cstdint>
#include <string>

#include "kalshi/logging/log_fields.hpp"
#include "kalshi/logging/log_level.hpp"

namespace kalshi::logging
{

  struct LogEvent
  {
    std::uint64_t ts_ms;
    LogLevel level;
    std::string component;
    std::string message;
    LogFields fields;
    std::string raw;
    bool include_raw = false;
  };

} // namespace kalshi::logging
