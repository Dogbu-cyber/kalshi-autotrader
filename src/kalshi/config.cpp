#include "kalshi/core/config.hpp"

#include <expected>
#include <fstream>
#include <optional>
#include <sstream>

#include <simdjson.h>

namespace kalshi
{

  namespace
  {

    std::expected<std::string, ConfigError>
    get_string(simdjson::ondemand::object &obj, std::string_view key)
    {
      auto field = obj[key];
      if (field.error())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }
      auto val = field.get_string();
      if (val.error())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }
      return std::string(val.value());
    }

    std::expected<std::optional<std::string>, ConfigError>
    get_optional_string(simdjson::ondemand::object &obj, std::string_view key)
    {
      auto field = obj[key];
      if (field.error())
      {
        if (field.error() == simdjson::NO_SUCH_FIELD)
        {
          return std::optional<std::string>{};
        }
        return std::unexpected(ConfigError::ParseFailed);
      }
      auto val = field.get_string();
      if (val.error())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }
      return std::optional<std::string>{std::string(val.value())};
    }

    std::expected<std::optional<std::size_t>, ConfigError>
    get_optional_size(simdjson::ondemand::object &obj, std::string_view key)
    {
      auto field = obj[key];
      if (field.error())
      {
        if (field.error() == simdjson::NO_SUCH_FIELD)
        {
          return std::optional<std::size_t>{};
        }
        return std::unexpected(ConfigError::ParseFailed);
      }
      auto val = field.get_uint64();
      if (val.error())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }
      return std::optional<std::size_t>{static_cast<std::size_t>(val.value())};
    }

    std::expected<std::optional<bool>, ConfigError>
    get_optional_bool(simdjson::ondemand::object &obj, std::string_view key)
    {
      auto field = obj[key];
      if (field.error())
      {
        if (field.error() == simdjson::NO_SUCH_FIELD)
        {
          return std::optional<bool>{};
        }
        return std::unexpected(ConfigError::ParseFailed);
      }
      auto val = field.get_bool();
      if (val.error())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }
      return std::optional<bool>{val.value()};
    }

    std::expected<std::vector<std::string>, ConfigError>
    get_string_array(simdjson::ondemand::object &obj, std::string_view key)
    {
      auto field = obj[key];
      if (field.error())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }
      auto arr = field.get_array();
      if (arr.error())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }

      std::vector<std::string> out;
      for (auto item : arr)
      {
        auto val = item.get_string();
        if (val.error())
        {
          return std::unexpected(ConfigError::ParseFailed);
        }
        out.emplace_back(std::string(val.value()));
      }
      return out;
    }

    std::expected<SubscriptionConfig, ConfigError>
    parse_subscription(simdjson::ondemand::object &root)
    {
      auto sub = root["subscription"].get_object();
      if (sub.error())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }

      auto channels = get_string_array(sub.value(), "channels");
      if (!channels || channels->empty())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }

      auto tickers = get_string_array(sub.value(), "market_tickers");
      if (!tickers)
      {
        return std::unexpected(ConfigError::ParseFailed);
      }

      return SubscriptionConfig{.channels = std::move(*channels),
                                .market_tickers = std::move(*tickers)};
    }

    std::expected<WsConfig, ConfigError>
    parse_ws(simdjson::ondemand::object &root, WsConfig base)
    {
      auto ws = root["ws"].get_object();
      if (ws.error())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }

      auto handshake_timeout_ms = get_optional_size(ws.value(), "handshake_timeout_ms");
      auto idle_timeout_ms = get_optional_size(ws.value(), "idle_timeout_ms");
      auto keep_alive_pings = get_optional_bool(ws.value(), "keep_alive_pings");
      auto auto_reconnect = get_optional_bool(ws.value(), "auto_reconnect");
      auto reconnect_initial_delay_ms =
          get_optional_size(ws.value(), "reconnect_initial_delay_ms");
      auto reconnect_max_delay_ms =
          get_optional_size(ws.value(), "reconnect_max_delay_ms");

      if (!handshake_timeout_ms || !idle_timeout_ms || !keep_alive_pings ||
          !auto_reconnect || !reconnect_initial_delay_ms || !reconnect_max_delay_ms)
      {
        return std::unexpected(ConfigError::ParseFailed);
      }

      if (handshake_timeout_ms->has_value())
      {
        base.handshake_timeout_ms =
            static_cast<std::int64_t>(**handshake_timeout_ms);
      }
      if (idle_timeout_ms->has_value())
      {
        base.idle_timeout_ms = static_cast<std::int64_t>(**idle_timeout_ms);
      }
      if (keep_alive_pings->has_value())
      {
        base.keep_alive_pings = **keep_alive_pings;
      }
      if (auto_reconnect->has_value())
      {
        base.auto_reconnect = **auto_reconnect;
      }
      if (reconnect_initial_delay_ms->has_value())
      {
        base.reconnect_initial_delay_ms =
            static_cast<std::int64_t>(**reconnect_initial_delay_ms);
      }
      if (reconnect_max_delay_ms->has_value())
      {
        base.reconnect_max_delay_ms =
            static_cast<std::int64_t>(**reconnect_max_delay_ms);
      }

      if (base.handshake_timeout_ms < 0 || base.idle_timeout_ms < 0 ||
          base.reconnect_initial_delay_ms < 0 || base.reconnect_max_delay_ms < 0)
      {
        return std::unexpected(ConfigError::ParseFailed);
      }
      if (base.reconnect_initial_delay_ms > base.reconnect_max_delay_ms)
      {
        return std::unexpected(ConfigError::ParseFailed);
      }

      return base;
    }

    std::expected<LoggingConfig, ConfigError>
    parse_logging(simdjson::ondemand::object &root, LoggingConfig base)
    {
      auto log = root["logging"].get_object();
      if (log.error())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }

      auto level = get_optional_string(log.value(), "level");
      auto queue_size = get_optional_size(log.value(), "queue_size");
      auto drop_policy = get_optional_string(log.value(), "drop_policy");
      auto include_raw =
          get_optional_bool(log.value(), "include_raw_on_parse_error");
      auto log_raw_messages = get_optional_bool(log.value(), "log_raw_messages");
      auto output_path = get_optional_string(log.value(), "output_path");

      if (!level || !queue_size || !drop_policy || !include_raw ||
          !log_raw_messages || !output_path)
      {
        return std::unexpected(ConfigError::ParseFailed);
      }

      if (level->has_value())
      {
        const auto &val = **level;
        if (val != "trace" && val != "debug" && val != "info" && val != "warn" &&
            val != "error")
        {
          return std::unexpected(ConfigError::ParseFailed);
        }
        base.level = std::move(**level);
      }

      if (queue_size->has_value())
      {
        if (**queue_size == 0)
        {
          return std::unexpected(ConfigError::ParseFailed);
        }
        base.queue_size = **queue_size;
      }

      if (drop_policy->has_value())
      {
        if (**drop_policy != "drop_oldest" && **drop_policy != "drop_newest")
        {
          return std::unexpected(ConfigError::ParseFailed);
        }
        base.drop_policy = std::move(**drop_policy);
      }

      if (include_raw->has_value())
      {
        base.include_raw_on_parse_error = **include_raw;
      }

      if (log_raw_messages->has_value())
      {
        base.log_raw_messages = **log_raw_messages;
      }

      if (output_path->has_value())
      {
        if ((*output_path)->empty())
        {
          return std::unexpected(ConfigError::ParseFailed);
        }
        base.output_path = std::move(**output_path);
      }

      return base;
    }

    std::expected<OutputConfig, ConfigError>
    parse_output(simdjson::ondemand::object &root)
    {
      auto out = root["output"].get_object();
      if (out.error())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }

      auto path = get_string(out.value(), "raw_messages_path");
      if (!path || path->empty())
      {
        return std::unexpected(ConfigError::ParseFailed);
      }

      return OutputConfig{.raw_messages_path = std::move(*path)};
    }

    LoggingConfig default_logging_config()
    {
      return LoggingConfig{.level = "info",
                           .queue_size = 10000,
                           .drop_policy = "drop_oldest",
                           .include_raw_on_parse_error = true,
                           .log_raw_messages = false,
                           .output_path = "logs/kalshi.log.json"};
    }

    OutputConfig default_output_config()
    {
      return OutputConfig{.raw_messages_path = "logs/ws_messages.json"};
    }

    WsConfig default_ws_config()
    {
      return WsConfig{.handshake_timeout_ms = 30000,
                      .idle_timeout_ms = 60000,
                      .keep_alive_pings = true,
                      .auto_reconnect = true,
                      .reconnect_initial_delay_ms = 500,
                      .reconnect_max_delay_ms = 30000};
    }

  } // namespace

  std::expected<Config, ConfigError> load_config(const std::string &path)
  {
    std::ifstream file(path);
    if (!file.is_open())
    {
      return std::unexpected(ConfigError::FileOpenFailed);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const auto content = buffer.str();

    simdjson::ondemand::parser parser;
    simdjson::padded_string padded(content);
    auto doc = parser.iterate(padded);
    if (doc.error())
    {
      return std::unexpected(ConfigError::ParseFailed);
    }

    auto root = doc.get_object();
    if (root.error())
    {
      return std::unexpected(ConfigError::ParseFailed);
    }

    auto env = get_string(root.value(), "env");
    auto ws_url = get_string(root.value(), "ws_url");
    auto subscription = parse_subscription(root.value());

    WsConfig ws_cfg = default_ws_config();
    if (auto ws = root.value()["ws"]; !ws.error())
    {
      auto parsed = parse_ws(root.value(), ws_cfg);
      if (!parsed)
      {
        return std::unexpected(ConfigError::ParseFailed);
      }
      ws_cfg = std::move(*parsed);
    }

    LoggingConfig logging = default_logging_config();
    if (auto log = root.value()["logging"]; !log.error())
    {
      auto parsed = parse_logging(root.value(), logging);
      if (!parsed)
      {
        return std::unexpected(ConfigError::ParseFailed);
      }
      logging = std::move(*parsed);
    }

    OutputConfig output = default_output_config();
    if (auto out = root.value()["output"]; !out.error())
    {
      auto parsed = parse_output(root.value());
      if (!parsed)
      {
        return std::unexpected(ConfigError::ParseFailed);
      }
      output = std::move(*parsed);
    }

    if (!env || !ws_url || !subscription)
    {
      return std::unexpected(ConfigError::ParseFailed);
    }

    return Config{.env = std::move(*env),
                  .ws_url = std::move(*ws_url),
                  .subscription = std::move(*subscription),
                  .ws = std::move(ws_cfg),
                  .logging = std::move(logging),
                  .output = std::move(output)};
  }

  std::string resolve_ws_url(const Config &config) { return config.ws_url; }

} // namespace kalshi
