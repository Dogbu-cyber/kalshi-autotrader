#include "kalshi/app/app_context.hpp"
#include "kalshi/app/logging_sink.hpp"
#include "kalshi/md/feed_handler.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

int main()
{
  auto ctx_result = kalshi::app::AppContext::build("config.json");
  if (!ctx_result)
  {
    return 1;
  }

  auto ctx = std::move(*ctx_result);
  kalshi::app::LoggingSink sink(ctx.logger());
  kalshi::md::FeedHandler handler(sink, ctx.logger());
  ctx.log_config();

  boost::asio::io_context ioc;
  boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
  ssl_ctx.set_default_verify_paths();
  ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);

  auto run_result = handler.run(ioc, ssl_ctx, ctx.build_run_options());
  if (!run_result)
  {
    ctx.logger().log(kalshi::logging::LogLevel::Error, "md.feed_handler", "run_failed");
    return 1;
  }
  return 0;
}
