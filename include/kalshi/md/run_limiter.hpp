#pragma once

#include <cstddef>

namespace kalshi::md
{

  class RunLimiter
  {
  public:
    explicit RunLimiter(std::size_t max_messages)
        : remaining_(max_messages), seen_(0) {}

    void on_message() { ++seen_; }

    [[nodiscard]] bool should_stop()
    {
      if (remaining_ == 0)
      {
        return false;
      }
      --remaining_;
      return remaining_ == 0;
    }

    [[nodiscard]] std::size_t seen() const { return seen_; }

  private:
    std::size_t remaining_;
    std::size_t seen_;
  };

} // namespace kalshi::md
