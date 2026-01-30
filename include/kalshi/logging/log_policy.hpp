#pragma once

#include <optional>
#include <string_view>

namespace kalshi::logging
{

  // Queue overflow policy for async logging.
  enum class DropPolicy
  {
    DropOldest,
    DropNewest
  };

  // Parse drop policy string.
  [[nodiscard]] std::optional<DropPolicy> parse_drop_policy(std::string_view text);

} // namespace kalshi::logging
