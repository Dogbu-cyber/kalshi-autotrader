#pragma once

namespace kalshi::md
{

  // Parsing errors for websocket message handling.
  enum class ParseError
  {
    EmptyMessage,
    InvalidJson,
    MissingType,
    MissingField,
    InvalidField,
    UnsupportedType
  };

} // namespace kalshi::md
