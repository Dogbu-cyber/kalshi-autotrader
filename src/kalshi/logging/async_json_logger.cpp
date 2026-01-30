#include "kalshi/logging/async_json_logger.hpp"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>
#include <type_traits>

#include "kalshi/logging/log_level.hpp"

namespace kalshi::logging
{

  namespace
  {

    std::uint64_t now_ms()
    {
      return static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count());
    }

    void append_json_string(std::string &out, std::string_view text)
    {
      for (char c : text)
      {
        switch (c)
        {
        case '\\':
          out += "\\\\";
          break;
        case '"':
          out += "\\\"";
          break;
        case '\b':
          out += "\\b";
          break;
        case '\f':
          out += "\\f";
          break;
        case '\n':
          out += "\\n";
          break;
        case '\r':
          out += "\\r";
          break;
        case '\t':
          out += "\\t";
          break;
        default:
          if (static_cast<unsigned char>(c) < 0x20)
          {
            std::ostringstream escaped;
            escaped << "\\u" << std::hex << std::uppercase << std::setw(4)
                    << std::setfill('0')
                    << static_cast<int>(static_cast<unsigned char>(c));
            out += escaped.str();
          }
          else
          {
            out += c;
          }
          break;
        }
      }
    }

    void append_field_value(std::string &out, const LogFieldValue &value)
    {
      std::visit(
          [&](const auto &val)
          {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::string>)
            {
              out += '"';
              append_json_string(out, val);
              out += '"';
            }
            else if constexpr (std::is_same_v<T, std::int64_t>)
            {
              out += std::to_string(val);
            }
            else if constexpr (std::is_same_v<T, std::uint64_t>)
            {
              out += std::to_string(val);
            }
            else if constexpr (std::is_same_v<T, double>)
            {
              out += std::to_string(val);
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
              out += (val ? "true" : "false");
            }
            else if constexpr (std::is_same_v<T, std::vector<std::string>>)
            {
              out += '[';
              for (std::size_t i = 0; i < val.size(); ++i)
              {
                if (i > 0)
                {
                  out += ',';
                }
                out += '"';
                append_json_string(out, val[i]);
                out += '"';
              }
              out += ']';
            }
          },
          value);
    }

  } // namespace

  AsyncJsonLogger::AsyncJsonLogger(AsyncJsonLoggerOptions options)
      : options_(std::move(options)), level_(options_.level),
        queue_size_(options_.queue_size), drop_policy_(options_.drop_policy),
        out_(nullptr)
  {
    ensure_output_path();
    file_.open(options_.output_path, std::ios::out | std::ios::app);
    if (!file_.is_open())
    {
      out_ = &std::cerr;
    }
    else
    {
      out_ = &file_;
    }
    worker_ = std::thread(&AsyncJsonLogger::run, this);
  }

  AsyncJsonLogger::~AsyncJsonLogger()
  {
    stop_.store(true);
    cv_.notify_all();
    if (worker_.joinable())
    {
      worker_.join();
    }
    if (file_.is_open())
    {
      file_.flush();
    }
  }

  void AsyncJsonLogger::log(LogEvent event)
  {
    if (!should_log(event.level, level_))
    {
      return;
    }
    if (event.ts_ms == 0)
    {
      event.ts_ms = now_ms();
    }
    enqueue(std::move(event));
  }

  void AsyncJsonLogger::enqueue(LogEvent event)
  {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.size() >= queue_size_)
    {
      if (drop_policy_ == DropPolicy::DropOldest)
      {
        queue_.pop_front();
        dropped_count_.fetch_add(1);
      }
      else
      {
        dropped_count_.fetch_add(1);
        return;
      }
    }
    queue_.push_back(std::move(event));
    cv_.notify_one();
  }

  void AsyncJsonLogger::run()
  {
    while (!stop_.load())
    {
      std::deque<LogEvent> batch;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]
                 { return stop_.load() || !queue_.empty(); });
        if (stop_.load() && queue_.empty())
        {
          break;
        }
        batch.swap(queue_);
      }

      if (!batch.empty())
      {
        write_batch(batch);
      }

      auto dropped = dropped_count_.exchange(0);
      if (dropped > 0)
      {
        write_dropped_summary(dropped);
      }

      if (out_)
      {
        out_->flush();
      }
    }

    if (!queue_.empty())
    {
      std::deque<LogEvent> remaining;
      {
        std::scoped_lock lock(mutex_);
        remaining.swap(queue_);
      }
      write_batch(remaining);
    }
  }

  void AsyncJsonLogger::write_batch(std::deque<LogEvent> &batch)
  {
    for (const auto &event : batch)
    {
      write_event(event);
    }
  }

  void AsyncJsonLogger::write_event(const LogEvent &event)
  {
    if (!out_)
    {
      return;
    }

    std::string line;
    line.reserve(256);
    line += "{\"ts_ms\":";
    line += std::to_string(event.ts_ms);
    line += ",\"level\":\"";
    append_json_string(line, to_string(event.level));
    line += "\",\"component\":\"";
    append_json_string(line, event.component);
    line += "\",\"msg\":\"";
    append_json_string(line, event.message);
    line += "\"";

    if (!event.fields.empty())
    {
      line += ",\"fields\":{";
      const auto &entries = event.fields.entries();
      for (std::size_t i = 0; i < entries.size(); ++i)
      {
        const auto &field = entries[i];
        if (i > 0)
        {
          line += ',';
        }
        line += '"';
        append_json_string(line, field.key);
        line += "\":";
        append_field_value(line, field.value);
      }
      line += '}';
    }

    if (event.include_raw)
    {
      line += ",\"raw\":\"";
      append_json_string(line, event.raw);
      line += "\"";
    }

    line += "}\n";
    (*out_) << line;
  }

  void AsyncJsonLogger::write_dropped_summary(std::uint64_t dropped)
  {
    LogFields fields;
    fields.add_uint("dropped", dropped);
    LogEvent event{.ts_ms = now_ms(),
                   .level = LogLevel::Warn,
                   .component = "logging",
                   .message = "dropped_logs",
                   .fields = std::move(fields),
                   .raw = {},
                   .include_raw = false};
    write_event(event);
  }

  void AsyncJsonLogger::ensure_output_path()
  {
    std::filesystem::path path(options_.output_path);
    if (!path.has_parent_path())
    {
      return;
    }
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
  }

} // namespace kalshi::logging
