#include "kalshi/md/ws/ws_client.hpp"

#include <utility>

#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>

#include <openssl/err.h>

#include "kalshi/md/ws/ws_constants.hpp"

namespace kalshi::md {

namespace {

bool starts_with(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

} // namespace

std::expected<WsUrl, WsError> parse_ws_url(std::string_view url) {
  if (!starts_with(url, WSS_PREFIX)) {
    return std::unexpected(WsError::InvalidUrl);
  }

  std::string_view rest = url.substr(WSS_PREFIX.size());
  auto slash = rest.find('/');
  std::string_view host_port = rest.substr(0, slash);
  std::string_view target = slash == std::string_view::npos ? "/" : rest.substr(slash);

  if (host_port.empty()) {
    return std::unexpected(WsError::InvalidUrl);
  }

  std::string_view host = host_port;
  std::string_view port = "443";
  auto colon = host_port.find(':');
  if (colon != std::string_view::npos) {
    host = host_port.substr(0, colon);
    port = host_port.substr(colon + 1);
  }

  if (host.empty() || port.empty()) {
    return std::unexpected(WsError::InvalidUrl);
  }

  return WsUrl{std::string(host), std::string(port), std::string(target)};
}

WsClient::WsClient(boost::asio::io_context& ioc, boost::asio::ssl::context& ssl_ctx)
  : resolver_(ioc),
    ws_(ioc, ssl_ctx) {}

void WsClient::set_message_callback(MessageCallback cb) { on_message_ = std::move(cb); }
void WsClient::set_error_callback(ErrorCallback cb) { on_error_ = std::move(cb); }
void WsClient::set_open_callback(OpenCallback cb) { on_open_ = std::move(cb); }

void WsClient::connect(const std::string& url, const std::vector<kalshi::Header>& headers) {
  auto parsed = parse_ws_url(url);
  if (!parsed) {
    fail(parsed.error(), "invalid url");
    return;
  }

  headers_ = headers;
  host_ = std::move(parsed->host);
  target_ = std::move(parsed->target);
  std::string port = parsed->port;

  configure_timeouts();
  configure_headers();
  resolve_host(std::move(port));
}

void WsClient::send_text(std::string payload) {
  ws_.async_write(boost::asio::buffer(payload),
                  boost::beast::bind_front_handler(&WsClient::on_write, this));
}

void WsClient::close() {
  boost::system::error_code ec;
  ws_.close(boost::beast::websocket::close_code::normal, ec);
}

void WsClient::on_resolve(boost::system::error_code ec,
                          boost::asio::ip::tcp::resolver::results_type results) {
  if (ec) {
    fail(WsError::ResolveFailed, ec.message());
    return;
  }

  boost::beast::get_lowest_layer(ws_).expires_after(CONNECT_TIMEOUT);
  boost::beast::get_lowest_layer(ws_).async_connect(
    results, boost::beast::bind_front_handler(&WsClient::on_connect, this));
}

void WsClient::on_connect(
  boost::system::error_code ec,
  boost::asio::ip::tcp::resolver::results_type::endpoint_type) {
  if (ec) {
    fail(WsError::ConnectFailed, ec.message());
    return;
  }

  if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str())) {
    boost::system::error_code ssl_ec{static_cast<int>(::ERR_get_error()),
                                     boost::asio::error::get_ssl_category()};
    fail(WsError::SslHandshakeFailed, ssl_ec.message());
    return;
  }

  ws_.next_layer().async_handshake(
    boost::asio::ssl::stream_base::client,
    boost::beast::bind_front_handler(&WsClient::on_ssl_handshake, this));
}

void WsClient::on_ssl_handshake(boost::system::error_code ec) {
  if (ec) {
    fail(WsError::SslHandshakeFailed, ec.message());
    return;
  }

  ws_.async_handshake(host_, target_,
                      boost::beast::bind_front_handler(&WsClient::on_ws_handshake, this));
}

void WsClient::on_ws_handshake(boost::system::error_code ec) {
  if (ec) {
    fail(WsError::WsHandshakeFailed, ec.message());
    return;
  }

  if (on_open_) {
    on_open_();
  }
  do_read();
}

void WsClient::do_read() {
  ws_.async_read(buffer_, boost::beast::bind_front_handler(&WsClient::on_read, this));
}

void WsClient::on_read(boost::system::error_code ec, std::size_t) {
  if (ec) {
    fail(WsError::ReadFailed, ec.message());
    return;
  }

  if (on_message_) {
    auto msg = boost::beast::buffers_to_string(buffer_.data());
    on_message_(std::move(msg));
  }
  buffer_.consume(buffer_.size());
  do_read();
}

void WsClient::on_write(boost::system::error_code ec, std::size_t) {
  if (ec) {
    fail(WsError::WriteFailed, ec.message());
  }
}

void WsClient::fail(WsError err, std::string_view msg) {
  if (on_error_) {
    on_error_(err, msg);
  }
}

void WsClient::configure_timeouts() {
  ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(
    boost::beast::role_type::client));
}

void WsClient::configure_headers() {
  ws_.set_option(boost::beast::websocket::stream_base::decorator(
    [this](boost::beast::websocket::request_type& req) {
      for (const auto& header : headers_) {
        req.set(header.first, header.second);
      }
    }));
}

void WsClient::resolve_host(std::string port) {
  resolver_.async_resolve(host_, port,
                          boost::beast::bind_front_handler(&WsClient::on_resolve, this));
}

} // namespace kalshi::md
