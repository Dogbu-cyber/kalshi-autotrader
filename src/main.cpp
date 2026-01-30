#include "kalshi/core/auth.hpp"
#include "kalshi/core/config.hpp"
#include "kalshi/md/feed_handler.hpp"
#include "kalshi/core/ws_endpoints.hpp"
#include "kalshi/md/protocol/message_types.hpp"
#include "kalshi/md/protocol/subscribe.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>

namespace {

struct LoggerSink {
  void on_snapshot(const kalshi::md::OrderbookSnapshot&) {
    std::cout << "snapshot" << std::endl;
  }
  void on_delta(const kalshi::md::OrderbookDelta&) { std::cout << "delta" << std::endl; }
  void on_trade(const kalshi::md::TradeEvent&) { std::cout << "trade" << std::endl; }
  void on_status(const kalshi::md::MarketStatusUpdate&) { std::cout << "status" << std::endl; }
};

std::optional<kalshi::Config> load_config_or_log() {
  auto config_result = kalshi::load_config("config.json");
  if (!config_result) {
    std::cout << "config load failed" << std::endl;
    return std::nullopt;
  }
  return std::move(*config_result);
}

std::optional<kalshi::AuthConfig> load_auth_or_log() {
  auto auth = kalshi::load_auth_from_env();
  if (!auth) {
    std::cout << "auth error: " << kalshi::to_string(auth.error()) << std::endl;
    return std::nullopt;
  }
  return std::move(*auth);
}

bool requires_market_tickers(const kalshi::Config& config) {
  return std::find(config.subscription.channels.begin(),
                   config.subscription.channels.end(),
                   kalshi::md::ORDERBOOK_DELTA) != config.subscription.channels.end();
}

std::string build_subscribe_command(const kalshi::Config& config) {
  kalshi::md::SubscribeRequest subscribe{.id = 1,
                                         .channels = config.subscription.channels,
                                         .market_tickers = config.subscription.market_tickers};
  return kalshi::md::build_subscribe_command(subscribe);
}

std::optional<std::vector<kalshi::Header>> build_headers_or_log(
  const kalshi::AuthConfig& auth) {
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch())
                  .count();
  auto built_headers = kalshi::build_ws_headers(auth, kalshi::WS_PATH, now_ms);
  if (!built_headers) {
    std::cout << "failed to build auth headers: " << kalshi::to_string(built_headers.error())
              << " (" << kalshi::last_sign_error() << ")" << std::endl;
    return std::nullopt;
  }
  return std::move(*built_headers);
}

} // namespace

int main() {
  auto config = load_config_or_log();
  if (!config) {
    return 1;
  }
  auto ws_url = kalshi::resolve_ws_url(*config);

  auto auth = load_auth_or_log();
  if (!auth) {
    return 1;
  }

  auto subscribe_cmd = build_subscribe_command(*config);

  if (requires_market_tickers(*config) && config->subscription.market_tickers.empty()) {
    std::cout << "orderbook_delta requires market_tickers in config.json" << std::endl;
    return 1;
  }

  LoggerSink logger;
  kalshi::md::FeedHandler handler(logger);

  kalshi::md::OrderbookSnapshot snap{.market_ticker = "EXAMPLE-TICKER",
                                     .sequence = 1,
                                     .yes = {},
                                     .no = {},
                                     .ts = std::chrono::nanoseconds{0}};

  handler.handle_snapshot(snap);
  std::cout << "ws_url: " << ws_url << std::endl;
  std::cout << "subscribe: " << subscribe_cmd << std::endl;

  auto built_headers = build_headers_or_log(*auth);
  if (!built_headers) {
    return 1;
  }

  boost::asio::io_context ioc;
  boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
  ssl_ctx.set_default_verify_paths();
  ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);

  kalshi::md::FeedHandler<LoggerSink>::RunOptions run_options{
    .ws_url = ws_url,
    .headers = *built_headers,
    .subscribe_cmd = subscribe_cmd,
    .output_path = config->output.raw_messages_path,
    .max_messages = 50
  };

  auto run_result = handler.run(ioc, ssl_ctx, std::move(run_options));
  if (!run_result) {
    std::cout << "feed handler run failed" << std::endl;
    return 1;
  }
  return 0;
}
