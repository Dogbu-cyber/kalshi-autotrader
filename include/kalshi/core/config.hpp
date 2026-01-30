#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <vector>

namespace kalshi {

enum class ConfigError {
  FileOpenFailed,
  ParseFailed
};

struct SubscriptionConfig {
  std::vector<std::string> channels;
  std::vector<std::string> market_tickers;
};

struct LoggingConfig {
  std::string level;
  std::size_t queue_size;
  std::string drop_policy;
  bool include_raw_on_parse_error;
};

struct OutputConfig {
  std::string raw_messages_path;
};

struct Config {
  std::string env;
  std::string ws_url;
  SubscriptionConfig subscription;
  LoggingConfig logging;
  OutputConfig output;
};

[[nodiscard]] std::expected<Config, ConfigError> load_config(const std::string& path);
[[nodiscard]] std::string resolve_ws_url(const Config& config);

} // namespace kalshi
