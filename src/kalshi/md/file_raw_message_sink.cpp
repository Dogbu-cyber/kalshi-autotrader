#include "kalshi/md/file_raw_message_sink.hpp"

#include <filesystem>

namespace kalshi::md
{

  FileRawMessageSink::FileRawMessageSink(std::string path)
      : out_()
  {
    std::filesystem::path fs_path(path);
    if (fs_path.has_parent_path())
    {
      std::error_code ec;
      std::filesystem::create_directories(fs_path.parent_path(), ec);
    }
    out_.open(std::move(path), std::ios::out);
  }

  void FileRawMessageSink::write(std::string_view message)
  {
    if (!out_.is_open())
    {
      return;
    }
    out_ << message << "\n";
    out_.flush();
  }

} // namespace kalshi::md
