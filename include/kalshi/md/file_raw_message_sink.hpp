#pragma once

#include "kalshi/md/raw_message_sink.hpp"

#include <fstream>
#include <string>

namespace kalshi::md
{

  class FileRawMessageSink final : public RawMessageSink
  {
  public:
    explicit FileRawMessageSink(std::string path);

    void write(std::string_view message) override;
    [[nodiscard]] bool ok() const { return out_.is_open(); }

  private:
    std::ofstream out_;
  };

} // namespace kalshi::md
