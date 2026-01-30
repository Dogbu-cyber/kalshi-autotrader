#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include "kalshi/core/auth.hpp"

namespace kalshi::md
{

  enum class WsError
  {
    InvalidUrl,
    ResolveFailed,
    ConnectFailed,
    SslHandshakeFailed,
    WsHandshakeFailed,
    ReadFailed,
    WriteFailed
  };

  struct WsUrl
  {
    std::string host;
    std::string port;
    std::string target;
  };

  [[nodiscard]] std::expected<WsUrl, WsError> parse_ws_url(std::string_view url);

  class WsClient
  {
  public:
    using MessageCallback = std::function<void(std::string)>;
    using ErrorCallback = std::function<void(WsError, std::string_view)>;
    using OpenCallback = std::function<void()>;

    WsClient(boost::asio::io_context &ioc, boost::asio::ssl::context &ssl_ctx);

    void set_message_callback(MessageCallback cb);
    void set_error_callback(ErrorCallback cb);
    void set_open_callback(OpenCallback cb);

    void connect(const std::string &url, const std::vector<kalshi::Header> &headers);
    void send_text(std::string payload);
    void close();

  private:
    void on_resolve(boost::system::error_code ec,
                    boost::asio::ip::tcp::resolver::results_type results);
    void on_connect(boost::system::error_code ec,
                    boost::asio::ip::tcp::resolver::results_type::endpoint_type);
    void on_ssl_handshake(boost::system::error_code ec);
    void on_ws_handshake(boost::system::error_code ec);
    void do_read();
    void on_read(boost::system::error_code ec, std::size_t bytes);
  void on_write(boost::system::error_code ec, std::size_t bytes);
  void fail(WsError err, std::string_view msg);
  void configure_timeouts();
  void configure_headers();
  void resolve_host(std::string port);

    boost::asio::ip::tcp::resolver resolver_;
    boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>>
        ws_;

    boost::beast::flat_buffer buffer_;
    std::vector<kalshi::Header> headers_;
    std::string host_;
    std::string target_;

    MessageCallback on_message_;
    ErrorCallback on_error_;
    OpenCallback on_open_;
  };

} // namespace kalshi::md
