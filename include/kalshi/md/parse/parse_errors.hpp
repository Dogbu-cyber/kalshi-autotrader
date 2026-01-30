#pragma once

namespace kalshi::md
{

  /** Parsing errors for websocket messages. */
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
