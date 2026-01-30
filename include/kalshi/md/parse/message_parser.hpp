#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "kalshi/md/parse/parse_errors.hpp"

namespace kalshi::md
{

  /** Extract message type string from a websocket JSON payload. */
  [[nodiscard]] std::expected<std::string, ParseError> parse_message_type(
      std::string_view json);

} // namespace kalshi::md
