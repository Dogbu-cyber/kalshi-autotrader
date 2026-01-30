#include "kalshi/core/auth.hpp"
#include "kalshi/core/config.hpp"
#include "kalshi/core/ws_endpoints.hpp"
#include "kalshi/logging/async_json_logger.hpp"
#include "kalshi/logging/log_level.hpp"
#include "kalshi/logging/log_policy.hpp"
#include "kalshi/md/feed_handler.hpp"
#include "kalshi/md/protocol/message_types.hpp"
#include "kalshi/md/protocol/subscribe.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>

namespace
{

  struct LoggingSink
  {
    kalshi::logging::Logger &logger;

    void on_snapshot(const kalshi::md::OrderbookSnapshot &snapshot)
    {
      kalshi::logging::LogFields fields;
      fields.add_string("market_ticker", snapshot.market_ticker);
      fields.add_uint("sequence", snapshot.sequence);
      logger.log(kalshi::logging::LogLevel::Info, "md.sink", "orderbook_snapshot",
                 std::move(fields));
    }

    void on_delta(const kalshi::md::OrderbookDelta &delta)
    {
      kalshi::logging::LogFields fields;
      fields.add_string("market_ticker", delta.market_ticker);
      fields.add_uint("sequence", delta.sequence);
      fields.add_uint("price", delta.price);
      fields.add_int("delta", delta.delta);
      logger.log(kalshi::logging::LogLevel::Debug, "md.sink", "orderbook_delta",
                 std::move(fields));
    }

    void on_trade(const kalshi::md::TradeEvent &trade)
    {
      kalshi::logging::LogFields fields;
      fields.add_string("market_ticker", trade.market_ticker);
      fields.add_uint("yes_price", trade.yes_price);
      fields.add_uint("no_price", trade.no_price);
      fields.add_uint("count", trade.count);
      logger.log(kalshi::logging::LogLevel::Debug, "md.sink", "trade",
                 std::move(fields));
    }

    void on_status(const kalshi::md::MarketStatusUpdate &status)
    {
      kalshi::logging::LogFields fields;
      fields.add_string("market_ticker", status.market_ticker);
      fields.add_uint("status", static_cast<std::uint64_t>(status.status));
      logger.log(kalshi::logging::LogLevel::Info, "md.sink", "market_status",
                 std::move(fields));
    }
  };

  std::optional<kalshi::Config> load_config_or_log()
  {
    auto config_result = kalshi::load_config("config.json");
    if (!config_result)
    {
      std::cerr << "config load failed" << std::endl;
      return std::nullopt;
    }
    return *config_result;
  }

  std::optional<kalshi::AuthConfig>
  load_auth_or_log(kalshi::logging::Logger &logger)
  {
    auto auth = kalshi::load_auth_from_env();
    if (!auth)
    {
      kalshi::logging::LogFields fields;
      fields.add_string("error", std::string(kalshi::to_string(auth.error())));
      logger.log(kalshi::logging::LogLevel::Error, "core.auth", "auth_error",
                 std::move(fields));
      return std::nullopt;
    }
    return *auth;
  }

  bool requires_market_tickers(const kalshi::Config &config)
  {
    return std::find(config.subscription.channels.begin(),
                     config.subscription.channels.end(),
                     kalshi::md::ORDERBOOK_DELTA) !=
           config.subscription.channels.end();
  }

  std::string build_subscribe_command(const kalshi::Config &config)
  {
    kalshi::md::SubscribeRequest subscribe{
        .id = 1,
        .channels = config.subscription.channels,
        .market_tickers = config.subscription.market_tickers};
    return kalshi::md::build_subscribe_command(subscribe);
  }

  std::optional<std::vector<kalshi::Header>>
  build_headers_or_log(const kalshi::AuthConfig &auth,
                       kalshi::logging::Logger &logger)
  {
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    auto built_headers = kalshi::build_ws_headers(auth, kalshi::WS_PATH, now_ms);
    if (!built_headers)
    {
      kalshi::logging::LogFields fields;
      fields.add_string("error",
                        std::string(kalshi::to_string(built_headers.error())));
      fields.add_string("openssl_error", kalshi::last_sign_error());
      logger.log(kalshi::logging::LogLevel::Error, "core.auth", "signing_failed",
                 std::move(fields));
      return std::nullopt;
    }
    return *built_headers;
  }

  std::optional<kalshi::logging::AsyncJsonLoggerOptions>
  build_logger_options(const kalshi::Config &config)
  {
    auto level = kalshi::logging::parse_log_level(config.logging.level);
    if (!level)
    {
      std::cerr << "invalid log level in config.json" << std::endl;
      return std::nullopt;
    }
    auto drop_policy =
        kalshi::logging::parse_drop_policy(config.logging.drop_policy);
    if (!drop_policy)
    {
      std::cerr << "invalid drop policy in config.json" << std::endl;
      return std::nullopt;
    }

    return kalshi::logging::AsyncJsonLoggerOptions{
        .level = *level,
        .queue_size = config.logging.queue_size,
        .drop_policy = *drop_policy,
        .output_path = config.logging.output_path};
  }

} // namespace

int main()
{
  auto config = load_config_or_log();
  if (!config)
  {
    return 1;
  }
  auto ws_url = kalshi::resolve_ws_url(*config);

  auto logger_options = build_logger_options(*config);
  if (!logger_options)
  {
    return 1;
  }
  kalshi::logging::AsyncJsonLogger logger(*logger_options);

  auto auth = load_auth_or_log(logger);
  if (!auth)
  {
    return 1;
  }

  auto subscribe_cmd = build_subscribe_command(*config);

  if (requires_market_tickers(*config) &&
      config->subscription.market_tickers.empty())
  {
    logger.log(kalshi::logging::LogLevel::Error, "core.config",
               "orderbook_delta_requires_market_tickers");
    return 1;
  }

  LoggingSink sink{.logger = logger};
  kalshi::md::FeedHandler handler(sink, logger);

  kalshi::logging::LogFields ws_fields;
  ws_fields.add_string("ws_url", ws_url);
  logger.log(kalshi::logging::LogLevel::Info, "core.config", "ws_url",
             std::move(ws_fields));

  kalshi::logging::LogFields sub_fields;
  sub_fields.add_string_list("channels", config->subscription.channels);
  sub_fields.add_string_list("market_tickers",
                             config->subscription.market_tickers);
  logger.log(kalshi::logging::LogLevel::Info, "core.config", "subscription",
             std::move(sub_fields));

  auto built_headers = build_headers_or_log(*auth, logger);
  if (!built_headers)
  {
    return 1;
  }

  boost::asio::io_context ioc;
  boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
  ssl_ctx.set_default_verify_paths();
  ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);

  kalshi::md::FeedHandler<LoggingSink>::RunOptions run_options{
      .ws_url = ws_url,
      .headers = *built_headers,
      .subscribe_cmd = subscribe_cmd,
      .output_path = config->output.raw_messages_path,
      .include_raw_on_parse_error = config->logging.include_raw_on_parse_error,
      .log_raw_messages = config->logging.log_raw_messages,
      .auto_reconnect = config->ws.auto_reconnect,
      .reconnect_initial_delay = std::chrono::milliseconds(
          config->ws.reconnect_initial_delay_ms),
      .reconnect_max_delay = std::chrono::milliseconds(
          config->ws.reconnect_max_delay_ms),
      .handshake_timeout = std::chrono::milliseconds(
          config->ws.handshake_timeout_ms),
      .idle_timeout = std::chrono::milliseconds(
          config->ws.idle_timeout_ms),
      .keep_alive_pings = config->ws.keep_alive_pings,
      .max_messages = 50};

  auto run_result = handler.run(ioc, ssl_ctx, std::move(run_options));
  if (!run_result)
  {
    logger.log(kalshi::logging::LogLevel::Error, "md.feed_handler",
               "run_failed");
    return 1;
  }
  return 0;
}
