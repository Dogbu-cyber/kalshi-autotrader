#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "kalshi/md/parse/parse_errors.hpp"

namespace kalshi::md
{

  /**
   * Extract message type string from a websocket JSON payload.
   * @param json Raw websocket message.
   * @return Message type string or ParseError.
   */
  [[nodiscard]] std::expected<std::string, ParseError> parse_message_type(
      std::string_view json);

} // namespace kalshi::md
