#include "kalshi/logging/log_policy.hpp"

namespace kalshi::logging
{

  std::optional<DropPolicy> parse_drop_policy(std::string_view text)
  {
    if (text == "drop_oldest")
    {
      return DropPolicy::DropOldest;
    }
    if (text == "drop_newest")
    {
      return DropPolicy::DropNewest;
    }
    return std::nullopt;
  }

} // namespace kalshi::logging
