#pragma once

#include "kalshi/core/config.hpp"

#include <expected>
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

  /** Errors returned while building subscription requests. */
  enum class SubscribeError
  {
    MissingMarketTickers
  };

  /**
   * Build a subscription request from config.
   * @param config Loaded config.
   * @param id Request id.
   * @return SubscribeRequest or SubscribeError.
   */
  [[nodiscard]] std::expected<SubscribeRequest, SubscribeError>
  build_subscribe_request(const kalshi::Config &config, int id);

  /**
   * Build JSON subscribe command from request.
   * @param req Subscribe request.
   * @return JSON payload string.
   */
  [[nodiscard]] std::string build_subscribe_command(const SubscribeRequest &req);

} // namespace kalshi::md
