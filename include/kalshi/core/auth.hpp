#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kalshi {

struct AuthConfig {
  std::string key_id;
  std::string private_key_pem;
};

enum class AuthError {
  MissingKeyId,
  MissingPrivateKey,
  SigningFailed
};

using Header = std::pair<std::string, std::string>;

[[nodiscard]] std::expected<AuthConfig, AuthError> load_auth_from_env();
[[nodiscard]] const char* to_string(AuthError error);
[[nodiscard]] const char* last_sign_error();
[[nodiscard]] std::expected<std::string, AuthError> sign_ws_message(
  std::string_view private_key_pem,
  std::string_view message);
[[nodiscard]] std::expected<std::vector<Header>, AuthError> build_ws_headers(
  const AuthConfig& auth,
  std::string_view path,
  std::int64_t timestamp_ms);

} // namespace kalshi
