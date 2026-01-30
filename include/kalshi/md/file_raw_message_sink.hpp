#pragma once

#include "kalshi/md/raw_message_sink.hpp"

#include <fstream>
#include <string>

namespace kalshi::md
{

  /** Raw message sink that appends JSON lines to a file. */
  class FileRawMessageSink final : public RawMessageSink
  {
  public:
    /** Open the output file and create parent directories if needed. */
    explicit FileRawMessageSink(std::string path);

    /** Write a message as a single JSON line. */
    void write(std::string_view message) override;
    /** Return true if the output file is open. */
    [[nodiscard]] bool ok() const { return out_.is_open(); }

  private:
    std::ofstream out_;
  };

} // namespace kalshi::md
