#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <vector>

namespace kalshi
{

  /** Errors returned while loading config.json. */
  enum class ConfigError
  {
    FileOpenFailed,
    ParseFailed
  };

  /** Websocket subscription parameters. */
  struct SubscriptionConfig
  {
    std::vector<std::string> channels;
    std::vector<std::string> market_tickers;
  };

  /** Websocket connection and reconnect configuration. */
  struct WsConfig
  {
    std::int64_t handshake_timeout_ms;
    std::int64_t idle_timeout_ms;
    bool keep_alive_pings;
    bool auto_reconnect;
    std::int64_t reconnect_initial_delay_ms;
    std::int64_t reconnect_max_delay_ms;
  };

  /** Logging configuration for async logger. */
  struct LoggingConfig
  {
    std::string level;
    std::size_t queue_size;
    std::string drop_policy;
    bool include_raw_on_parse_error;
    bool log_raw_messages;
    std::string output_path;
  };

  /** Output destinations for raw message capture. */
  struct OutputConfig
  {
    std::string raw_messages_path;
  };

  /** Top-level runtime configuration loaded from config.json. */
  struct Config
  {
    std::string env;
    std::string ws_url;
    SubscriptionConfig subscription;
    WsConfig ws;
    LoggingConfig logging;
    OutputConfig output;
  };

  /**
   * Load and parse config.json.
   * @param path Path to config.json.
   * @return Parsed Config or ConfigError.
   */
  [[nodiscard]] std::expected<Config, ConfigError> load_config(const std::string &path);
  /**
   * Resolve final websocket URL for the configured environment.
   * @param config Loaded config.
   * @return Resolved websocket URL.
   */
  [[nodiscard]] std::string resolve_ws_url(const Config &config);

} // namespace kalshi
