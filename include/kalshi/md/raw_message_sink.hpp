#pragma once

#include <string_view>

namespace kalshi::md
{

  class RawMessageSink
  {
  public:
    virtual ~RawMessageSink() = default;
    virtual void write(std::string_view message) = 0;
  };

} // namespace kalshi::md
