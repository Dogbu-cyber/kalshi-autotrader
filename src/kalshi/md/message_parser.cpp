#include "kalshi/md/parse/message_parser.hpp"

#include "kalshi/md/parse/json_fields.hpp"

#include <simdjson.h>

namespace kalshi::md {

std::expected<std::string, ParseError>
parse_message_type(std::string_view json) {
  if (json.empty()) {
    return std::unexpected(ParseError::EmptyMessage);
  }

  simdjson::ondemand::parser parser;
  simdjson::padded_string padded(json.data(), json.size());
  auto doc = parser.iterate(padded);
  if (doc.error()) {
    return std::unexpected(ParseError::InvalidJson);
  }

  auto type_field = doc[FIELD_TYPE];
  if (type_field.error()) {
    return std::unexpected(ParseError::MissingType);
  }

  auto type_str = type_field.get_string();
  if (type_str.error()) {
    return std::unexpected(ParseError::MissingType);
  }

  return std::string(type_str.value());
}

} // namespace kalshi::md
