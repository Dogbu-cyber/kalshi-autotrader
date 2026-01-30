#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace kalshi::md
{

  /** Subscribe command payload. */
  struct SubscribeRequest
  {
    int id;
    std::vector<std::string> channels;
    std::vector<std::string> market_tickers;
  };

  /** Build JSON subscribe command from request. */
  [[nodiscard]] std::string build_subscribe_command(const SubscribeRequest &req);

} // namespace kalshi::md
