#include "kalshi/app/app_context.hpp"

#include <chrono>
#include <iostream>
#include <optional>

namespace kalshi::app
{

std::expected<AppContext, AppError> AppContext::build(std::string_view config_path)
{
  auto config_result = kalshi::load_config(std::string(config_path));
  if (!config_result)
  {
    std::cerr << "config load failed" << std::endl;
    return std::unexpected(AppError::ConfigLoadFailed);
  }

  auto logger_options = build_logger_options(*config_result);
  if (!logger_options)
  {
    return std::unexpected(logger_options.error());
  }

  auto logger = std::make_unique<kalshi::logging::AsyncJsonLogger>(*logger_options);

  auto auth = kalshi::load_auth_from_env();
  if (!auth)
  {
    kalshi::logging::LogFields fields;
    fields.add_string("error", std::string(kalshi::to_string(auth.error())));
    logger->log(kalshi::logging::LogLevel::Error, "core.auth", "auth_error", std::move(fields));
    return std::unexpected(AppError::AuthLoadFailed);
  }

  auto ws_url = kalshi::resolve_ws_url(*config_result);

  auto subscription = kalshi::md::SubscriptionCommand::from_config(*config_result, 1);
  if (!subscription)
  {
    logger->log(
        kalshi::logging::LogLevel::Error, "core.config", "orderbook_delta_requires_market_tickers");
    return std::unexpected(AppError::SubscriptionInvalid);
  }

  auto headers = build_headers(*auth, *logger);
  if (!headers)
  {
    return std::unexpected(headers.error());
  }

  return AppContext(std::move(*config_result),
                    std::move(logger),
                    std::move(*auth),
                    std::move(*headers),
                    std::move(*subscription),
                    std::move(ws_url));
}

kalshi::md::FeedHandler<LoggingSink>::RunOptions AppContext::build_run_options() const
{
  auto refresh = [this]() -> std::optional<std::vector<kalshi::Header>>
  {
    auto refreshed = build_headers(auth_, *logger_);
    if (!refreshed)
    {
      return std::nullopt;
    }
    return std::move(*refreshed);
  };

  return kalshi::md::FeedHandler<LoggingSink>::RunOptions{
      .ws_url = ws_url_,
      .headers = headers_,
      .refresh_headers = refresh,
      .subscribe_cmd = subscription_.json(),
      .output_path = config_.output.raw_messages_path,
      .include_raw_on_parse_error = config_.logging.include_raw_on_parse_error,
      .log_raw_messages = config_.logging.log_raw_messages,
      .auto_reconnect = config_.ws.auto_reconnect,
      .reconnect_initial_delay = std::chrono::milliseconds(config_.ws.reconnect_initial_delay_ms),
      .reconnect_max_delay = std::chrono::milliseconds(config_.ws.reconnect_max_delay_ms),
      .handshake_timeout = std::chrono::milliseconds(config_.ws.handshake_timeout_ms),
      .idle_timeout = std::chrono::milliseconds(config_.ws.idle_timeout_ms),
      .keep_alive_pings = config_.ws.keep_alive_pings,
      .max_messages = 0};
}

void AppContext::log_config() const
{
  kalshi::logging::LogFields ws_fields;
  ws_fields.add_string("ws_url", ws_url_);
  logger_->log(kalshi::logging::LogLevel::Info, "core.config", "ws_url", std::move(ws_fields));

  kalshi::logging::LogFields sub_fields;
  sub_fields.add_string_list("channels", subscription_.request().channels);
  sub_fields.add_string_list("market_tickers", subscription_.request().market_tickers);
  logger_->log(
      kalshi::logging::LogLevel::Info, "core.config", "subscription", std::move(sub_fields));
}

std::expected<kalshi::logging::AsyncJsonLoggerOptions, AppError> AppContext::build_logger_options(
    const kalshi::Config& config)
{
  auto level = kalshi::logging::parse_log_level(config.logging.level);
  if (!level)
  {
    std::cerr << "invalid log level in config.json" << std::endl;
    return std::unexpected(AppError::InvalidLogLevel);
  }
  auto drop_policy = kalshi::logging::parse_drop_policy(config.logging.drop_policy);
  if (!drop_policy)
  {
    std::cerr << "invalid drop policy in config.json" << std::endl;
    return std::unexpected(AppError::InvalidDropPolicy);
  }

  return kalshi::logging::AsyncJsonLoggerOptions{.level = *level,
                                                 .queue_size = config.logging.queue_size,
                                                 .drop_policy = *drop_policy,
                                                 .output_path = config.logging.output_path};
}

std::expected<std::vector<kalshi::Header>, AppError> AppContext::build_headers(
    const kalshi::AuthConfig& auth, kalshi::logging::Logger& logger)
{
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  auto built_headers = kalshi::build_ws_headers(auth, kalshi::WS_PATH, now_ms);
  if (!built_headers)
  {
    kalshi::logging::LogFields fields;
    fields.add_string("error", std::string(kalshi::to_string(built_headers.error())));
    fields.add_string("openssl_error", kalshi::last_sign_error());
    logger.log(kalshi::logging::LogLevel::Error, "core.auth", "signing_failed", std::move(fields));
    return std::unexpected(AppError::SigningFailed);
  }
  return *built_headers;
}

} // namespace kalshi::app
