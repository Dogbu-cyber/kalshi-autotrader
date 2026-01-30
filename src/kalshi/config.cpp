#include "kalshi/core/config.hpp"

#include <expected>
#include <fstream>
#include <sstream>

#include <simdjson.h>

namespace kalshi {

namespace {

std::expected<std::string, ConfigError> get_string(simdjson::ondemand::object& obj,
                                                   std::string_view key) {
  auto field = obj[key];
  if (field.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }
  auto val = field.get_string();
  if (val.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }
  return std::string(val.value());
}

std::expected<std::size_t, ConfigError> get_size(simdjson::ondemand::object& obj,
                                                 std::string_view key) {
  auto field = obj[key];
  if (field.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }
  auto val = field.get_uint64();
  if (val.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }
  return static_cast<std::size_t>(val.value());
}

std::expected<bool, ConfigError> get_bool(simdjson::ondemand::object& obj,
                                          std::string_view key) {
  auto field = obj[key];
  if (field.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }
  auto val = field.get_bool();
  if (val.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }
  return val.value();
}

std::expected<std::vector<std::string>, ConfigError> get_string_array(
  simdjson::ondemand::object& obj,
  std::string_view key) {
  auto field = obj[key];
  if (field.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }
  auto arr = field.get_array();
  if (arr.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }

  std::vector<std::string> out;
  for (auto item : arr) {
    auto val = item.get_string();
    if (val.error()) {
      return std::unexpected(ConfigError::ParseFailed);
    }
    out.emplace_back(std::string(val.value()));
  }
  return out;
}

std::expected<SubscriptionConfig, ConfigError> parse_subscription(
  simdjson::ondemand::object& root) {
  auto sub = root["subscription"].get_object();
  if (sub.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }

  auto channels = get_string_array(sub.value(), "channels");
  if (!channels || channels->empty()) {
    return std::unexpected(ConfigError::ParseFailed);
  }

  auto tickers = get_string_array(sub.value(), "market_tickers");
  if (!tickers) {
    return std::unexpected(ConfigError::ParseFailed);
  }

  return SubscriptionConfig{.channels = std::move(*channels),
                            .market_tickers = std::move(*tickers)};
}

std::expected<LoggingConfig, ConfigError> parse_logging(simdjson::ondemand::object& root) {
  auto log = root["logging"].get_object();
  if (log.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }

  auto level = get_string(log.value(), "level");
  auto queue_size = get_size(log.value(), "queue_size");
  auto drop_policy = get_string(log.value(), "drop_policy");
  auto include_raw = get_bool(log.value(), "include_raw_on_parse_error");

  if (!level || !queue_size || !drop_policy || !include_raw) {
    return std::unexpected(ConfigError::ParseFailed);
  }

  if (*queue_size == 0) {
    return std::unexpected(ConfigError::ParseFailed);
  }

  if (*drop_policy != "drop_oldest" && *drop_policy != "drop_newest") {
    return std::unexpected(ConfigError::ParseFailed);
  }

  return LoggingConfig{.level = std::move(*level),
                       .queue_size = *queue_size,
                       .drop_policy = std::move(*drop_policy),
                       .include_raw_on_parse_error = *include_raw};
}

std::expected<OutputConfig, ConfigError> parse_output(simdjson::ondemand::object& root) {
  auto out = root["output"].get_object();
  if (out.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }

  auto path = get_string(out.value(), "raw_messages_path");
  if (!path || path->empty()) {
    return std::unexpected(ConfigError::ParseFailed);
  }

  return OutputConfig{.raw_messages_path = std::move(*path)};
}

LoggingConfig default_logging_config() {
  return LoggingConfig{.level = "info",
                       .queue_size = 10000,
                       .drop_policy = "drop_oldest",
                       .include_raw_on_parse_error = true};
}

OutputConfig default_output_config() {
  return OutputConfig{.raw_messages_path = "logs/ws_messages.json"};
}

} // namespace

std::expected<Config, ConfigError> load_config(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return std::unexpected(ConfigError::FileOpenFailed);
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  const auto content = buffer.str();

  simdjson::ondemand::parser parser;
  simdjson::padded_string padded(content);
  auto doc = parser.iterate(padded);
  if (doc.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }

  auto root = doc.get_object();
  if (root.error()) {
    return std::unexpected(ConfigError::ParseFailed);
  }

  auto env = get_string(root.value(), "env");
  auto ws_url = get_string(root.value(), "ws_url");
  auto subscription = parse_subscription(root.value());

  LoggingConfig logging = default_logging_config();
  if (auto log = root.value()["logging"]; !log.error()) {
    auto parsed = parse_logging(root.value());
    if (!parsed) {
      return std::unexpected(ConfigError::ParseFailed);
    }
    logging = std::move(*parsed);
  }

  OutputConfig output = default_output_config();
  if (auto out = root.value()["output"]; !out.error()) {
    auto parsed = parse_output(root.value());
    if (!parsed) {
      return std::unexpected(ConfigError::ParseFailed);
    }
    output = std::move(*parsed);
  }

  if (!env || !ws_url || !subscription) {
    return std::unexpected(ConfigError::ParseFailed);
  }

  return Config{.env = std::move(*env),
                .ws_url = std::move(*ws_url),
                .subscription = std::move(*subscription),
                .logging = std::move(logging),
                .output = std::move(output)};
}

std::string resolve_ws_url(const Config& config) { return config.ws_url; }

} // namespace kalshi
