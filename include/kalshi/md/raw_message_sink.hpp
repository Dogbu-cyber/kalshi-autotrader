#pragma once

#include <string_view>

namespace kalshi::md
{

  /** Interface for writing raw websocket messages. */
  /**
   * Interface for writing raw websocket messages.
   */
  class RawMessageSink
  {
  public:
    virtual ~RawMessageSink() = default;
    /**
     * Persist a single raw websocket message.
     * @param message Raw websocket message.
     * @return void.
     */
    virtual void write(std::string_view message) = 0;
  };

} // namespace kalshi::md
