#pragma once

#include "kalshi/md/dispatcher.hpp"
#include "kalshi/md/model/market_sink.hpp"
#include "kalshi/md/ws/ws_client.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <cstddef>
#include <expected>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace kalshi::md
{

  template <MarketSink Sink>
  class FeedHandler
  {
  public:
    explicit FeedHandler(Sink &sink) : sink_(sink) {}

    void handle_snapshot(const OrderbookSnapshot &s) { sink_.on_snapshot(s); }
    void handle_delta(const OrderbookDelta &d) { sink_.on_delta(d); }
    void handle_trade(const TradeEvent &t) { sink_.on_trade(t); }
    void handle_status(const MarketStatusUpdate &u) { sink_.on_status(u); }

    enum class RunError
    {
      OutputOpenFailed,
      OutputPathInvalid
    };

    struct RunOptions
    {
      std::string ws_url;
      std::vector<kalshi::Header> headers;
      std::string subscribe_cmd;
      std::string output_path;
      std::size_t max_messages = 0; // 0 = unlimited
    };

    struct RunState
    {
      std::shared_ptr<std::ofstream> out;
      std::shared_ptr<std::size_t> remaining;
      std::shared_ptr<std::size_t> seen;
      std::shared_ptr<Dispatcher<Sink>> dispatcher;
      std::string subscribe_cmd;
    };

    [[nodiscard]] std::expected<void, RunError> run(boost::asio::io_context &ioc,
                                                    boost::asio::ssl::context &ssl_ctx,
                                                    RunOptions options)
    {
      auto state = init_state(options);
      if (!state) {
        return std::unexpected(RunError::OutputOpenFailed);
      }

      WsClient client(ioc, ssl_ctx);
      WsClient *client_ptr = &client;
      configure_callbacks(client, ioc, *state, client_ptr);

      client.connect(options.ws_url, options.headers);
      ioc.run();
      return {};
    }

  private:
    [[nodiscard]] std::optional<RunState> init_state(const RunOptions &options)
    {
      RunState state;
      std::filesystem::path path(options.output_path);
      if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
          return std::nullopt;
        }
      }

      state.out = std::make_shared<std::ofstream>(options.output_path, std::ios::out);
      if (!state.out->is_open())
      {
        return std::nullopt;
      }
      state.remaining = std::make_shared<std::size_t>(options.max_messages);
      state.seen = std::make_shared<std::size_t>(0);
      state.dispatcher = std::make_shared<Dispatcher<Sink>>(sink_);
      state.subscribe_cmd = options.subscribe_cmd;
      return state;
    }

    void configure_callbacks(WsClient &client,
                             boost::asio::io_context &ioc,
                             RunState &state,
                             WsClient *client_ptr)
    {
      client.set_open_callback([client_ptr, cmd = std::move(state.subscribe_cmd)]() mutable
                               { on_open(client_ptr, cmd); });
      client.set_message_callback([this, &ioc, &state, client_ptr](std::string msg)
                                  { on_message(ioc, state, client_ptr, std::move(msg)); });
      client.set_error_callback([&](WsError err, std::string_view msg)
                                { on_error(ioc, err, msg); });
    }

    static void on_open(WsClient *client_ptr, std::string &cmd)
    {
      std::cout << "ws open" << std::endl;
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
      (*state.out) << msg << "\n";
      state.out->flush();
      if (*state.seen == 0)
      {
        std::cout << "first message received" << std::endl;
      }
      ++(*state.seen);

      auto dispatched = state.dispatcher->on_message(msg);
      if (!dispatched && dispatched.error() != ParseError::UnsupportedType)
      {
        std::cout << "parse error" << std::endl;
      }

      if (*state.remaining > 0)
      {
        --(*state.remaining);
        if (*state.remaining == 0)
        {
          client_ptr->close();
          ioc.stop();
        }
      }
    }

    static void on_error(boost::asio::io_context &ioc, WsError err, std::string_view msg)
    {
      std::cout << "ws error: " << static_cast<int>(err) << " " << msg << std::endl;
      ioc.stop();
    }

    Sink &sink_;
  };

} // namespace kalshi::md
