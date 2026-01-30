#pragma once

#include <optional>
#include <string_view>

namespace kalshi::logging
{

  enum class DropPolicy
  {
    DropOldest,
    DropNewest
  };

  [[nodiscard]] std::optional<DropPolicy> parse_drop_policy(std::string_view text);

} // namespace kalshi::logging
