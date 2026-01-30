#pragma once

#include "kalshi/md/dispatcher.hpp"
#include "kalshi/md/model/market_sink.hpp"
#include "kalshi/md/ws/ws_client.hpp"
#include "kalshi/logging/logger.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <cstdint>
#include <cstddef>
#include <expected>
#include <fstream>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kalshi::md
{

  // Owns websocket connection and dispatches messages to a sink.
  template <MarketSink Sink>
  class FeedHandler
  {
  public:
    FeedHandler(Sink &sink, kalshi::logging::Logger &logger)
        : sink_(sink), logger_(logger) {}

    void handle_snapshot(const OrderbookSnapshot &s) { sink_.on_snapshot(s); }
    void handle_delta(const OrderbookDelta &d) { sink_.on_delta(d); }
    void handle_trade(const TradeEvent &t) { sink_.on_trade(t); }
    void handle_status(const MarketStatusUpdate &u) { sink_.on_status(u); }

    // Errors during startup or output initialization.
    enum class RunError
    {
      OutputOpenFailed,
      OutputPathInvalid
    };

    // Runtime options for websocket run.
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

    // Shared state for callbacks across async operations.
    struct RunState
    {
      std::shared_ptr<std::ofstream> out;
      std::shared_ptr<std::size_t> remaining;
      std::shared_ptr<std::size_t> seen;
      std::shared_ptr<Dispatcher<Sink>> dispatcher;
      std::string subscribe_cmd;
      bool include_raw_on_parse_error = true;
      bool log_raw_messages = false;
    };

    // Start websocket loop and dispatch messages until stopped.
    [[nodiscard]] std::expected<void, RunError> run(boost::asio::io_context &ioc,
                                                    boost::asio::ssl::context &ssl_ctx,
                                                    RunOptions options)
    {
      auto state_result = init_state(options);
      if (!state_result)
      {
        if (state_result.error() == RunError::OutputPathInvalid)
        {
          log_error("md.feed_handler", "output_path_invalid");
        }
        else
        {
          log_error("md.feed_handler", "output_open_failed");
        }
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
    // Validate output path and construct shared run state.
    [[nodiscard]] std::expected<RunState, RunError> init_state(const RunOptions &options)
    {
      RunState state;
      std::filesystem::path path(options.output_path);
      if (path.has_parent_path())
      {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
        {
          return std::unexpected(RunError::OutputPathInvalid);
        }
      }

      state.out = std::make_shared<std::ofstream>(options.output_path, std::ios::out);
      if (!state.out->is_open())
      {
        return std::unexpected(RunError::OutputOpenFailed);
      }
      state.remaining = std::make_shared<std::size_t>(options.max_messages);
      state.seen = std::make_shared<std::size_t>(0);
      state.dispatcher = std::make_shared<Dispatcher<Sink>>(sink_);
      state.subscribe_cmd = options.subscribe_cmd;
      state.include_raw_on_parse_error = options.include_raw_on_parse_error;
      state.log_raw_messages = options.log_raw_messages;
      return state;
    }

    // Wire websocket callbacks to handler methods.
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

    // Send subscribe command after connection opens.
    void on_open(WsClient *client_ptr, std::string &cmd)
    {
      log_info("md.ws_client", "ws_open");
      if (!cmd.empty())
      {
        client_ptr->send_text(std::move(cmd));
      }
    }

    // Persist raw message, dispatch parsed events, and honor max_messages.
    void on_message(boost::asio::io_context &ioc,
                    RunState &state,
                    WsClient *client_ptr,
                    std::string msg)
    {
      (*state.out) << msg << "\n";
      state.out->flush();
      if (state.log_raw_messages)
      {
        kalshi::logging::LogFields fields;
        fields.add_uint("bytes", static_cast<std::uint64_t>(msg.size()));
        fields.add_uint("count", static_cast<std::uint64_t>(*state.seen));
        logger_.log_raw(kalshi::logging::LogLevel::Debug, "md.ws_client",
                        "ws_message", std::move(fields), msg);
      }
      if (*state.seen == 0)
      {
        log_info("md.feed_handler", "first_message_received");
      }
      ++(*state.seen);

      auto dispatched = state.dispatcher->on_message(msg);
      if (!dispatched && dispatched.error() != ParseError::UnsupportedType)
      {
        log_parse_error("md.dispatcher", dispatched.error(), msg, state.include_raw_on_parse_error);
      }
      if (!dispatched && dispatched.error() == ParseError::UnsupportedType)
      {
        log_debug("md.dispatcher", "unsupported_message_type");
      }

      if (*state.remaining > 0)
      {
        --(*state.remaining);
        if (*state.remaining == 0)
        {
          log_info("md.feed_handler", "max_messages_reached");
          client_ptr->close();
          ioc.stop();
        }
      }
    }

    // Log error and stop IO loop.
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

    void log_parse_error(std::string_view component,
                         ParseError error,
                         const std::string &raw,
                         bool include_raw)
    {
      kalshi::logging::LogFields fields;
      fields.add_int("parse_error", static_cast<std::int64_t>(error));
      if (include_raw)
      {
        logger_.log_raw(kalshi::logging::LogLevel::Warn, std::string(component),
                        "parse_error", std::move(fields), raw);
        return;
      }
      logger_.log(kalshi::logging::LogLevel::Warn, std::string(component),
                  "parse_error", std::move(fields));
    }
  };

} // namespace kalshi::md
