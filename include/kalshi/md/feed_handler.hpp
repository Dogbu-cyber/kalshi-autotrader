#pragma once

#include "kalshi/md/model/market_sink.hpp"
#include "kalshi/md/file_raw_message_sink.hpp"
#include "kalshi/md/message_pipeline.hpp"
#include "kalshi/md/run_limiter.hpp"
#include "kalshi/md/ws/ws_client.hpp"
#include "kalshi/md/ws/ws_constants.hpp"
#include "kalshi/logging/logger.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <algorithm>
#include <chrono>

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
      std::size_t max_messages = 0; // 0 = unlimited
    };

    /**
     * Shared state for async callbacks.
     */
    struct RunState
    {
      std::string subscribe_cmd;
      std::shared_ptr<MessagePipeline<Sink>> pipeline;
      std::shared_ptr<RunLimiter> limiter;
      std::shared_ptr<WsClient> client;
      std::shared_ptr<boost::asio::steady_timer> reconnect_timer;
      std::string ws_url;
      std::vector<kalshi::Header> headers;
      bool auto_reconnect = true;
      std::chrono::milliseconds reconnect_initial_delay{500};
      std::chrono::milliseconds reconnect_delay{500};
      std::chrono::milliseconds reconnect_max_delay{30000};
      boost::asio::io_context *ioc = nullptr;
      boost::asio::ssl::context *ssl_ctx = nullptr;
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
      auto state_result = init_state(options);
      if (!state_result)
      {
        log_error("md.feed_handler", "output_open_failed");
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
    [[nodiscard]] std::expected<RunState, RunError> init_state(const RunOptions &options)
    {
      auto raw_sink = std::make_unique<FileRawMessageSink>(options.output_path);
      if (!raw_sink->ok())
      {
        return std::unexpected(RunError::OutputOpenFailed);
      }

      RunState state{.subscribe_cmd = options.subscribe_cmd,
                     .pipeline = std::make_shared<MessagePipeline<Sink>>(
                         sink_, logger_, std::move(raw_sink),
                         options.include_raw_on_parse_error,
                         options.log_raw_messages),
                     .limiter = std::make_shared<RunLimiter>(options.max_messages),
                     .client = nullptr,
                     .reconnect_timer = nullptr,
                     .ws_url = options.ws_url,
                     .headers = options.headers,
                     .auto_reconnect = options.auto_reconnect,
                     .reconnect_initial_delay = options.reconnect_initial_delay,
                     .reconnect_delay = options.reconnect_initial_delay,
                     .reconnect_max_delay = options.reconnect_max_delay,
                     .ioc = nullptr,
                     .ssl_ctx = nullptr};
      return state;
    }

    void connect_client(RunState &state)
    {
      state.client = std::make_shared<WsClient>(*state.ioc, *state.ssl_ctx);
      configure_callbacks(*state.client, *state.ioc, state);
      state.client->connect(state.ws_url, state.headers);
    }

    void configure_callbacks(WsClient &client,
                             boost::asio::io_context &ioc,
                             RunState &state)
    {
      client.set_open_callback([this, &state]() mutable
                               { on_open(state); });
      client.set_message_callback([this, &ioc, &state](std::string msg)
                                  { on_message(ioc, state, std::move(msg)); });
      client.set_error_callback([this, &ioc, &state](WsError err, std::string_view msg)
                                { on_error(ioc, state, err, msg); });
      client.set_control_callback([this](boost::beast::websocket::frame_type kind,
                                         std::string_view payload)
                                  { on_control(kind, payload); });
      client.set_timeouts(CONNECT_TIMEOUT, IDLE_TIMEOUT, true);
    }

    void on_open(RunState &state)
    {
      log_info("md.ws_client", "ws_open");
      state.reconnect_delay = state.reconnect_initial_delay;
      if (!state.subscribe_cmd.empty())
      {
        state.client->send_text(state.subscribe_cmd);
      }
    }

    void on_message(boost::asio::io_context &ioc,
                    RunState &state,
                    std::string msg)
    {
      if (state.limiter->seen() == 0)
      {
        log_info("md.feed_handler", "first_message_received");
      }
      state.limiter->on_message();
      state.pipeline->on_message(msg);

      if (state.limiter->should_stop())
      {
        log_info("md.feed_handler", "max_messages_reached");
        state.client->close();
        ioc.stop();
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
      if (!state.auto_reconnect)
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
      logger_.log(kalshi::logging::LogLevel::Debug, "md.ws_client",
                  kind == boost::beast::websocket::frame_type::ping
                      ? "ws_ping"
                      : kind == boost::beast::websocket::frame_type::pong ? "ws_pong"
                                                                         : "ws_control",
                  std::move(fields));
    }

    void schedule_reconnect(boost::asio::io_context &ioc, RunState &state)
    {
      if (!state.reconnect_timer)
      {
        state.reconnect_timer = std::make_shared<boost::asio::steady_timer>(ioc);
      }
      auto delay = state.reconnect_delay;
      if (state.reconnect_delay < state.reconnect_max_delay)
      {
        state.reconnect_delay =
            std::min(state.reconnect_delay * 2, state.reconnect_max_delay);
      }

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

    Sink &sink_;
    kalshi::logging::Logger &logger_;

    void log_info(std::string_view component, std::string_view message)
    {
      logger_.log(kalshi::logging::LogLevel::Info, std::string(component),
                  std::string(message));
    }

    void log_debug(std::string_view component, std::string_view message)
    {
      logger_.log(kalshi::logging::LogLevel::Debug, std::string(component),
                  std::string(message));
    }

    void log_error(std::string_view component, std::string_view message)
    {
      logger_.log(kalshi::logging::LogLevel::Error, std::string(component),
                  std::string(message));
    }

  };

} // namespace kalshi::md
