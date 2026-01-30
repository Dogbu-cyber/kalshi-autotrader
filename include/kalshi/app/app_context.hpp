#pragma once

#include "kalshi/app/logging_sink.hpp"
#include "kalshi/core/auth.hpp"
#include "kalshi/core/config.hpp"
#include "kalshi/core/ws_endpoints.hpp"
#include "kalshi/logging/async_json_logger.hpp"
#include "kalshi/logging/log_level.hpp"
#include "kalshi/logging/log_policy.hpp"
#include "kalshi/md/feed_handler.hpp"
#include "kalshi/md/protocol/subscribe.hpp"

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace kalshi::app
{

/**
 * Errors encountered while building app context.
 */
enum class AppError
{
  ConfigLoadFailed,
  InvalidLogLevel,
  InvalidDropPolicy,
  AuthLoadFailed,
  SigningFailed,
  SubscriptionInvalid
};

/**
 * Application context assembled from config and environment.
 */
class AppContext
{
public:
  /**
   * Build application context from a config path.
   * @param config_path Path to config file.
   * @return AppContext or AppError.
   */
  [[nodiscard]] static std::expected<AppContext, AppError> build(std::string_view config_path);

  /**
   * Access the configured logger.
   * @return Logger reference.
   */
  [[nodiscard]] kalshi::logging::Logger& logger() const
  {
    return *logger_;
  }

  /**
   * Access the loaded config.
   * @return Config reference.
   */
  [[nodiscard]] const kalshi::Config& config() const
  {
    return config_;
  }

  /**
   * Access the validated subscription.
   * @return Subscription command.
   */
  [[nodiscard]] const kalshi::md::SubscriptionCommand& subscription() const
  {
    return subscription_;
  }

  /**
   * Access the websocket URL.
   * @return Websocket URL string.
   */
  [[nodiscard]] const std::string& ws_url() const
  {
    return ws_url_;
  }

  /**
   * Build feed handler options.
   * @return Feed handler run options.
   */
  [[nodiscard]] kalshi::md::FeedHandler<LoggingSink>::RunOptions build_run_options() const;

  /**
   * Log config summary to the logger.
   */
  void log_config() const;

private:
  AppContext(kalshi::Config config,
             std::unique_ptr<kalshi::logging::AsyncJsonLogger> logger,
             kalshi::AuthConfig auth,
             std::vector<kalshi::Header> headers,
             kalshi::md::SubscriptionCommand subscription,
             std::string ws_url)
    : config_(std::move(config)),
      logger_(std::move(logger)),
      auth_(std::move(auth)),
      headers_(std::move(headers)),
      subscription_(std::move(subscription)),
      ws_url_(std::move(ws_url))
  {
  }

  [[nodiscard]] static std::expected<kalshi::logging::AsyncJsonLoggerOptions, AppError>
  build_logger_options(const kalshi::Config& config);

  [[nodiscard]] static std::expected<std::vector<kalshi::Header>, AppError> build_headers(
      const kalshi::AuthConfig& auth, kalshi::logging::Logger& logger);

  kalshi::Config config_;
  std::unique_ptr<kalshi::logging::AsyncJsonLogger> logger_;
  kalshi::AuthConfig auth_;
  std::vector<kalshi::Header> headers_;
  kalshi::md::SubscriptionCommand subscription_;
  std::string ws_url_;
};

} // namespace kalshi::app
