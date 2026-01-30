#pragma once

namespace kalshi::md
{

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
