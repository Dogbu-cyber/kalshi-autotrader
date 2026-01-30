#pragma once

#include <cstddef>

namespace kalshi::md
{

  /**
   * Tracks message counts and stop conditions for a run loop.
   */
  class RunLimiter
  {
  public:
    /**
     * Construct with max_messages (0 = unlimited).
     * @param max_messages Maximum messages to process.
     */
    explicit RunLimiter(std::size_t max_messages)
        : remaining_(max_messages), seen_(0) {}

    /**
     * Increment the seen message count.
     * @return void.
     */
    void on_message() { ++seen_; }

    /**
     * Return true if the run should stop after this message.
     * @return True if limit reached.
     */
    [[nodiscard]] bool should_stop()
    {
      if (remaining_ == 0)
      {
        return false;
      }
      --remaining_;
      return remaining_ == 0;
    }

    /**
     * Return total messages seen so far.
     * @return Seen message count.
     */
    [[nodiscard]] std::size_t seen() const { return seen_; }

  private:
    std::size_t remaining_;
    std::size_t seen_;
  };

} // namespace kalshi::md
