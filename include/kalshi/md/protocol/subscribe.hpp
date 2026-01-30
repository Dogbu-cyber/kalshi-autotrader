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
 * Subscription command with validated request and JSON payload.
 */
class SubscriptionCommand
{
public:
  /**
   * Build a validated subscription command from config.
   * @param config Loaded config.
   * @param id Request id.
   * @return SubscriptionCommand or SubscribeError.
   */
  [[nodiscard]] static std::expected<SubscriptionCommand, SubscribeError> from_config(
      const kalshi::Config& config, int id);

  /**
   * Create from a validated request.
   * @param request Subscribe request.
   */
  explicit SubscriptionCommand(SubscribeRequest request);

  /**
   * Return the JSON payload for websocket subscribe.
   * @return JSON payload string.
   */
  [[nodiscard]] const std::string& json() const
  {
    return json_;
  }

  /**
   * Access the validated request.
   * @return SubscribeRequest.
   */
  [[nodiscard]] const SubscribeRequest& request() const
  {
    return request_;
  }

private:
  [[nodiscard]] static std::expected<SubscribeRequest, SubscribeError> build_request(
      const kalshi::Config& config, int id);

  [[nodiscard]] static std::string build_json(const SubscribeRequest& req);

  SubscribeRequest request_;
  std::string json_;
};

} // namespace kalshi::md
