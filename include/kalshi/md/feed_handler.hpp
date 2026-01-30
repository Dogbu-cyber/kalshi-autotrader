#pragma once

#include "kalshi/md/model/market_sink.hpp"
#include "kalshi/md/file_raw_message_sink.hpp"
#include "kalshi/md/message_pipeline.hpp"
#include "kalshi/md/run_limiter.hpp"
#include "kalshi/md/ws/ws_client.hpp"
#include "kalshi/logging/logger.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kalshi::md
{

  template <MarketSink Sink>
  /** Owns websocket connection and dispatches messages to a sink. */
  class FeedHandler
  {
  public:
    /** Construct with a market sink and logger. */
    FeedHandler(Sink &sink, kalshi::logging::Logger &logger)
        : sink_(sink), logger_(logger) {}

    /** Errors returned by run(). */
    enum class RunError
    {
      OutputOpenFailed
    };

    /** Runtime options for websocket run. */
    struct RunOptions
    {
      std::string ws_url;
      std::vector<kalshi::Header> headers;
      std::string subscribe_cmd;
      std::string output_path;
      bool include_raw_on_parse_error = true;
      bool log_raw_messages = false;
      std::size_t max_messages = 0; // 0 = unlimited
    };

    /** Shared state for async callbacks. */
    struct RunState
    {
      std::string subscribe_cmd;
      std::shared_ptr<MessagePipeline<Sink>> pipeline;
      std::shared_ptr<RunLimiter> limiter;
    };

    /** Start websocket loop and dispatch messages until stopped. */
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
      WsClient client(ioc, ssl_ctx);
      WsClient *client_ptr = &client;
      configure_callbacks(client, ioc, state, client_ptr);

      client.connect(options.ws_url, options.headers);
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
                     .limiter = std::make_shared<RunLimiter>(options.max_messages)};
      return state;
    }

    void configure_callbacks(WsClient &client,
                             boost::asio::io_context &ioc,
                             RunState &state,
                             WsClient *client_ptr)
    {
      client.set_open_callback([this, client_ptr, cmd = std::move(state.subscribe_cmd)]() mutable
                               { on_open(client_ptr, cmd); });
      client.set_message_callback([this, &ioc, &state, client_ptr](std::string msg)
                                  { on_message(ioc, state, client_ptr, std::move(msg)); });
      client.set_error_callback([this, &ioc](WsError err, std::string_view msg)
                                { on_error(ioc, err, msg); });
    }

    void on_open(WsClient *client_ptr, std::string &cmd)
    {
      log_info("md.ws_client", "ws_open");
      if (!cmd.empty())
      {
        client_ptr->send_text(std::move(cmd));
      }
    }

    void on_message(boost::asio::io_context &ioc,
                    RunState &state,
                    WsClient *client_ptr,
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
        client_ptr->close();
        ioc.stop();
      }
    }

    void on_error(boost::asio::io_context &ioc, WsError err, std::string_view msg)
    {
      kalshi::logging::LogFields fields;
      fields.add_int("code", static_cast<std::int64_t>(err));
      fields.add_string("message", std::string(msg));
      logger_.log(kalshi::logging::LogLevel::Error, "md.ws_client", "ws_error",
                  std::move(fields));
      ioc.stop();
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
