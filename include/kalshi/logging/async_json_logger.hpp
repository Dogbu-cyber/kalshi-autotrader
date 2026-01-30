#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

#include "kalshi/logging/log_event.hpp"
#include "kalshi/logging/log_policy.hpp"
#include "kalshi/logging/logger.hpp"

namespace kalshi::logging
{

  struct AsyncJsonLoggerOptions
  {
    LogLevel level = LogLevel::Info;
    std::size_t queue_size = 10000;
    DropPolicy drop_policy = DropPolicy::DropOldest;
    std::string output_path = "logs/kalshi.log.json";
  };

  class AsyncJsonLogger : public Logger
  {
  public:
    explicit AsyncJsonLogger(AsyncJsonLoggerOptions options);
    ~AsyncJsonLogger() override;

    using Logger::log;

    AsyncJsonLogger(const AsyncJsonLogger &) = delete;
    AsyncJsonLogger &operator=(const AsyncJsonLogger &) = delete;
    AsyncJsonLogger(AsyncJsonLogger &&) = delete;
    AsyncJsonLogger &operator=(AsyncJsonLogger &&) = delete;

    void log(LogEvent event) override;
    [[nodiscard]] LogLevel level() const override { return level_; }

  private:
    void run();
    void enqueue(LogEvent event);
    void write_batch(std::deque<LogEvent> &batch);
    void write_event(const LogEvent &event);
    void write_dropped_summary(std::uint64_t dropped);
    void ensure_output_path();

    AsyncJsonLoggerOptions options_;
    LogLevel level_;
    std::size_t queue_size_;
    DropPolicy drop_policy_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<LogEvent> queue_;
    std::atomic<bool> stop_{false};
    std::atomic<std::uint64_t> dropped_count_{0};

    std::ofstream file_;
    std::ostream *out_;
    std::thread worker_;
  };

} // namespace kalshi::logging
