#pragma once

#include "kalshi/logging/logger.hpp"
#include "kalshi/md/dispatcher.hpp"
#include "kalshi/md/raw_message_sink.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace kalshi::md
{

  template <MarketSink Sink>
  /** Routes raw websocket messages through sinks, parsing, and logging. */
  class MessagePipeline
  {
  public:
    /** Construct a pipeline with optional raw sink and logging behavior. */
    MessagePipeline(Sink &sink,
                    kalshi::logging::Logger &logger,
                    std::unique_ptr<RawMessageSink> raw_sink,
                    bool include_raw_on_parse_error,
                    bool log_raw_messages)
        : dispatcher_(sink), logger_(logger), raw_sink_(std::move(raw_sink)),
          include_raw_on_parse_error_(include_raw_on_parse_error),
          log_raw_messages_(log_raw_messages) {}

    /** Process a single websocket message. */
    void on_message(std::string_view message)
    {
      if (raw_sink_)
      {
        raw_sink_->write(message);
      }

      if (log_raw_messages_)
      {
        kalshi::logging::LogFields fields;
        fields.add_uint("bytes", static_cast<std::uint64_t>(message.size()));
        logger_.log_raw(kalshi::logging::LogLevel::Debug, "md.ws_client",
                        "ws_message", std::move(fields), std::string(message));
      }

      auto dispatched = dispatcher_.on_message(message);
      if (!dispatched && dispatched.error() != ParseError::UnsupportedType)
      {
        log_parse_error(dispatched.error(), message);
      }
      if (!dispatched && dispatched.error() == ParseError::UnsupportedType)
      {
        logger_.log(kalshi::logging::LogLevel::Debug, "md.dispatcher",
                    "unsupported_message_type");
      }
    }

  private:
    void log_parse_error(ParseError error, std::string_view raw)
    {
      kalshi::logging::LogFields fields;
      fields.add_int("parse_error", static_cast<std::int64_t>(error));
      if (include_raw_on_parse_error_)
      {
        logger_.log_raw(kalshi::logging::LogLevel::Warn, "md.dispatcher",
                        "parse_error", std::move(fields), std::string(raw));
        return;
      }
      logger_.log(kalshi::logging::LogLevel::Warn, "md.dispatcher", "parse_error",
                  std::move(fields));
    }

    Dispatcher<Sink> dispatcher_;
    kalshi::logging::Logger &logger_;
    std::unique_ptr<RawMessageSink> raw_sink_;
    bool include_raw_on_parse_error_;
    bool log_raw_messages_;
  };

} // namespace kalshi::md
