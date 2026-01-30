#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kalshi
{

  /** Authentication configuration loaded from environment variables. */
  struct AuthConfig
  {
    std::string key_id;
    std::string private_key_pem;
  };

  /** Errors returned by auth loading and signing. */
  enum class AuthError
  {
    MissingKeyId,
    MissingPrivateKey,
    SigningFailed
  };

  using Header = std::pair<std::string, std::string>;

  /** Load API key id and private key from environment variables. */
  [[nodiscard]] std::expected<AuthConfig, AuthError> load_auth_from_env();
  /** Convert AuthError to a string literal. */
  [[nodiscard]] const char *to_string(AuthError error);
  /** Return the last OpenSSL error string from signing. */
  [[nodiscard]] const char *last_sign_error();
  /** Sign websocket request string using RSA-PSS. */
  [[nodiscard]] std::expected<std::string, AuthError> sign_ws_message(
      std::string_view private_key_pem,
      std::string_view message);
  /** Build Kalshi websocket auth headers. */
  [[nodiscard]] std::expected<std::vector<Header>, AuthError> build_ws_headers(
      const AuthConfig &auth,
      std::string_view path,
      std::int64_t timestamp_ms);

} // namespace kalshi
