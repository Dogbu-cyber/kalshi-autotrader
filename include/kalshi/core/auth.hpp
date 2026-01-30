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

  /**
   * Load API key id and private key from environment variables.
   * @return AuthConfig on success, otherwise AuthError.
   */
  [[nodiscard]] std::expected<AuthConfig, AuthError> load_auth_from_env();
  /**
   * Convert AuthError to a string literal.
   * @param error Error to stringify.
   * @return String literal describing the error.
   */
  [[nodiscard]] const char *to_string(AuthError error);
  /**
   * Return the last OpenSSL error string from signing.
   * @return Null-terminated error message.
   */
  [[nodiscard]] const char *last_sign_error();
  /**
   * Sign websocket request string using RSA-PSS.
   * @param private_key_pem Private key contents in PEM or OpenSSH format.
   * @param message Request string to sign.
   * @return Base64-encoded signature or AuthError.
   */
  [[nodiscard]] std::expected<std::string, AuthError> sign_ws_message(
      std::string_view private_key_pem,
      std::string_view message);
  /**
   * Build Kalshi websocket auth headers.
   * @param auth Auth configuration.
   * @param path Websocket path used for signing.
   * @param timestamp_ms Unix timestamp in milliseconds.
   * @return Header list or AuthError.
   */
  [[nodiscard]] std::expected<std::vector<Header>, AuthError> build_ws_headers(
      const AuthConfig &auth,
      std::string_view path,
      std::int64_t timestamp_ms);

} // namespace kalshi
