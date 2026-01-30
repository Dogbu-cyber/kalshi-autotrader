#pragma once

#include "kalshi/logging/logger.hpp"
#include "kalshi/md/dispatcher.hpp"
#include "kalshi/md/model/market_sink.hpp"
#include "kalshi/md/ws/ws_client.hpp"
#include "kalshi/md/ws/ws_constants.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kalshi::md
{

  template <MarketSink Sink>
  /**
   * Owns websocket connection and dispatches messages to a sink.
   */
  class FeedHandler
  {
  public:
    /**
     * Construct with a market sink and logger.
     * @param sink Market event sink.
     * @param logger Logger for diagnostics.
     */
    FeedHandler(Sink &sink, kalshi::logging::Logger &logger)
        : sink_(sink), logger_(logger) {}

    /**
     * Errors returned by run().
     */
    enum class RunError
    {
      OutputOpenFailed
    };

    /**
     * Runtime options for websocket run.
     */
    struct RunOptions
    {
      std::string ws_url;
      std::vector<kalshi::Header> headers;
      std::string subscribe_cmd;
      std::string output_path;
      bool include_raw_on_parse_error = true;
      bool log_raw_messages = false;
      bool auto_reconnect = true;
      std::chrono::milliseconds reconnect_initial_delay{500};
      std::chrono::milliseconds reconnect_max_delay{30000};
      std::chrono::milliseconds handshake_timeout{30000};
      std::chrono::milliseconds idle_timeout{60000};
      bool keep_alive_pings = true;
      std::size_t max_messages = 0; // 0 = unlimited
    };

    /**
     * Start websocket loop and dispatch messages until stopped.
     * @param ioc IO context for async operations.
     * @param ssl_ctx SSL context for TLS.
     * @param options Run options.
     * @return std::expected<void, RunError>.
     */
    [[nodiscard]] std::expected<void, RunError> run(boost::asio::io_context &ioc,
                                                    boost::asio::ssl::context &ssl_ctx,
                                                    RunOptions options)
    {
      auto state_result = init_state(std::move(options));
      if (!state_result)
      {
        log(kalshi::logging::LogLevel::Error, "md.feed_handler", "output_open_failed");
        return std::unexpected(state_result.error());
      }

      auto state = std::move(*state_result);
      state.ioc = &ioc;
      state.ssl_ctx = &ssl_ctx;
      connect_client(state);
      ioc.run();
      return {};
    }

  private:
    struct ReconnectState
    {
      bool enabled = true;
      std::chrono::milliseconds initial{500};
      std::chrono::milliseconds max{30000};
      std::chrono::milliseconds current{500};

      ReconnectState() = default;
      ReconnectState(bool enabled,
                     std::chrono::milliseconds initial,
                     std::chrono::milliseconds max)
          : enabled(enabled), initial(initial), max(max), current(initial) {}

      void reset() { current = initial; }

      std::chrono::milliseconds next_delay()
      {
        auto delay = current;
        if (current < max)
        {
          current = std::min(current * 2, max);
        }
        return delay;
      }
    };

    struct RunState
    {
      RunOptions options;
      std::shared_ptr<std::ofstream> out;
      std::size_t remaining = 0;
      std::size_t seen = 0;
      std::shared_ptr<WsClient> client;
      std::shared_ptr<boost::asio::steady_timer> reconnect_timer;
      ReconnectState reconnect;
      boost::asio::io_context *ioc = nullptr;
      boost::asio::ssl::context *ssl_ctx = nullptr;
    };

    [[nodiscard]] std::expected<RunState, RunError> init_state(RunOptions options)
    {
      std::filesystem::path path(options.output_path);
      if (path.has_parent_path())
      {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
        {
          return std::unexpected(RunError::OutputOpenFailed);
        }
      }

      auto out = std::make_shared<std::ofstream>(options.output_path, std::ios::out);
      if (!out->is_open())
      {
        return std::unexpected(RunError::OutputOpenFailed);
      }

      auto remaining = options.max_messages;
      auto reconnect = ReconnectState(options.auto_reconnect,
                                      options.reconnect_initial_delay,
                                      options.reconnect_max_delay);
      RunState state{.options = std::move(options),
                     .out = std::move(out),
                     .remaining = remaining,
                     .seen = 0,
                     .client = nullptr,
                     .reconnect_timer = nullptr,
                     .reconnect = reconnect,
                     .ioc = nullptr,
                     .ssl_ctx = nullptr};
      return state;
    }

    void connect_client(RunState &state)
    {
      state.client = std::make_shared<WsClient>(*state.ioc, *state.ssl_ctx);
      configure_callbacks(*state.client, *state.ioc, state);
      state.client->connect(state.options.ws_url, state.options.headers);
    }

    void configure_callbacks(WsClient &client,
                             boost::asio::io_context &ioc,
                             RunState &state)
    {
      client.set_open_callback([this, &state]() mutable { on_open(state); });
      client.set_message_callback([this, &ioc, &state](std::string msg) {
        on_message(ioc, state, std::move(msg));
      });
      client.set_error_callback([this, &ioc, &state](WsError err, std::string_view msg) {
        on_error(ioc, state, err, msg);
      });
      client.set_control_callback([this](boost::beast::websocket::frame_type kind,
                                         std::string_view payload) {
        on_control(kind, payload);
      });
      client.set_timeouts(
          std::chrono::duration_cast<std::chrono::seconds>(state.options.handshake_timeout),
          std::chrono::duration_cast<std::chrono::seconds>(state.options.idle_timeout),
          state.options.keep_alive_pings);
    }

    void on_open(RunState &state)
    {
      log(kalshi::logging::LogLevel::Info, "md.ws_client", "ws_open");
      state.reconnect.reset();
      if (!state.options.subscribe_cmd.empty())
      {
        state.client->send_text(state.options.subscribe_cmd);
      }
    }

    void on_message(boost::asio::io_context &ioc,
                    RunState &state,
                    std::string msg)
    {
      (*state.out) << msg << "\n";
      state.out->flush();

      if (state.options.log_raw_messages)
      {
        kalshi::logging::LogFields fields;
        fields.add_uint("bytes", static_cast<std::uint64_t>(msg.size()));
        fields.add_uint("count", static_cast<std::uint64_t>(state.seen));
        logger_.log_raw(kalshi::logging::LogLevel::Debug, "md.ws_client",
                        "ws_message", std::move(fields), msg);
      }

      if (state.seen == 0)
      {
        log(kalshi::logging::LogLevel::Info, "md.feed_handler", "first_message_received");
      }
      ++state.seen;

      auto dispatched = dispatch_message(msg, state.options.include_raw_on_parse_error);
      if (!dispatched && dispatched.error() == ParseError::UnsupportedType)
      {
        log(kalshi::logging::LogLevel::Debug, "md.dispatcher", "unsupported_message_type");
      }

      if (state.remaining > 0)
      {
        --state.remaining;
        if (state.remaining == 0)
        {
          log(kalshi::logging::LogLevel::Info, "md.feed_handler", "max_messages_reached");
          state.client->close();
          ioc.stop();
        }
      }
    }

    void on_error(boost::asio::io_context &ioc,
                  RunState &state,
                  WsError err,
                  std::string_view msg)
    {
      kalshi::logging::LogFields fields;
      fields.add_int("code", static_cast<std::int64_t>(err));
      fields.add_string("message", std::string(msg));
      logger_.log(kalshi::logging::LogLevel::Error, "md.ws_client", "ws_error",
                  std::move(fields));
      if (!state.reconnect.enabled)
      {
        ioc.stop();
        return;
      }
      schedule_reconnect(ioc, state);
    }

    void on_control(boost::beast::websocket::frame_type kind,
                    std::string_view payload)
    {
      kalshi::logging::LogFields fields;
      fields.add_string("payload", std::string(payload));
      logger_.log(kalshi::logging::LogLevel::Info, "md.ws_client",
                  kind == boost::beast::websocket::frame_type::ping
                      ? "ws_ping"
                      : kind == boost::beast::websocket::frame_type::pong
                            ? "ws_pong"
                            : "ws_control",
                  std::move(fields));
    }

    void schedule_reconnect(boost::asio::io_context &ioc, RunState &state)
    {
      if (!state.reconnect_timer)
      {
        state.reconnect_timer = std::make_shared<boost::asio::steady_timer>(ioc);
      }
      auto delay = state.reconnect.next_delay();

      kalshi::logging::LogFields fields;
      fields.add_int("delay_ms", static_cast<std::int64_t>(delay.count()));
      logger_.log(kalshi::logging::LogLevel::Warn, "md.ws_client",
                  "reconnect_scheduled", std::move(fields));

      state.reconnect_timer->expires_after(delay);
      state.reconnect_timer->async_wait([this, &state](const boost::system::error_code &ec) {
        if (ec)
        {
          return;
        }
        connect_client(state);
      });
    }

    std::expected<void, ParseError> dispatch_message(std::string_view msg,
                                                     bool include_raw_on_parse_error)
    {
      Dispatcher<Sink> dispatcher(sink_);
      auto dispatched = dispatcher.on_message(msg);
      if (!dispatched && dispatched.error() != ParseError::UnsupportedType)
      {
        log_parse_error(dispatched.error(), msg, include_raw_on_parse_error);
      }
      return dispatched;
    }

    void log_parse_error(ParseError error,
                         std::string_view raw,
                         bool include_raw)
    {
      kalshi::logging::LogFields fields;
      fields.add_int("parse_error", static_cast<std::int64_t>(error));
      if (include_raw)
      {
        logger_.log_raw(kalshi::logging::LogLevel::Warn, "md.dispatcher",
                        "parse_error", std::move(fields), std::string(raw));
        return;
      }
      logger_.log(kalshi::logging::LogLevel::Warn, "md.dispatcher",
                  "parse_error", std::move(fields));
    }

    void log(kalshi::logging::LogLevel level,
             std::string_view component,
             std::string_view message)
    {
      logger_.log(level, std::string(component), std::string(message));
    }

    Sink &sink_;
    kalshi::logging::Logger &logger_;
  };

} // namespace kalshi::md
