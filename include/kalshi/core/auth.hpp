#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kalshi
{

  // Authentication config loaded from environment variables.
  struct AuthConfig
  {
    std::string key_id;
    std::string private_key_pem;
  };

  // Errors raised while loading keys or signing.
  enum class AuthError
  {
    MissingKeyId,
    MissingPrivateKey,
    SigningFailed
  };

  using Header = std::pair<std::string, std::string>;

  // Load API key id and private key from env vars.
  [[nodiscard]] std::expected<AuthConfig, AuthError> load_auth_from_env();
  // Convert error enum to string.
  [[nodiscard]] const char *to_string(AuthError error);
  // Last OpenSSL error (thread-local).
  [[nodiscard]] const char *last_sign_error();
  // Sign the websocket request string using RSA-PSS.
  [[nodiscard]] std::expected<std::string, AuthError> sign_ws_message(
      std::string_view private_key_pem,
      std::string_view message);
  // Build websocket auth headers.
  [[nodiscard]] std::expected<std::vector<Header>, AuthError> build_ws_headers(
      const AuthConfig &auth,
      std::string_view path,
      std::int64_t timestamp_ms);

} // namespace kalshi
