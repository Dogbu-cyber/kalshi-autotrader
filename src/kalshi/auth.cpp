#include "kalshi/core/auth.hpp"

#include <cstdlib>
#include <expected>
#include <fstream>
#include <memory>
#include <sstream>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/provider.h>
#include <openssl/rsa.h>

namespace kalshi {

namespace {

std::string read_file(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return {};
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

thread_local std::string g_last_sign_error;

void ensure_legacy_provider_loaded() {
  static bool loaded = false;
  if (loaded) {
    return;
  }
  OSSL_PROVIDER_load(nullptr, "default");
  OSSL_PROVIDER_load(nullptr, "legacy");
  loaded = true;
}

void set_last_sign_error_from_openssl() {
  unsigned long code = ERR_get_error();
  if (code == 0) {
    g_last_sign_error = "unknown OpenSSL error";
    return;
  }
  char buffer[256];
  ERR_error_string_n(code, buffer, sizeof(buffer));
  g_last_sign_error = buffer;
}

bool is_openssh_key(std::string_view pem) {
  return pem.find("BEGIN OPENSSH PRIVATE KEY") != std::string_view::npos;
}

using PkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using MdCtxPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

std::expected<PkeyPtr, AuthError> load_private_key(std::string_view pem) {
  BIO* key_bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
  if (!key_bio) {
    set_last_sign_error_from_openssl();
    return std::unexpected(AuthError::SigningFailed);
  }

  EVP_PKEY* pkey = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
  BIO_free(key_bio);
  if (!pkey) {
    set_last_sign_error_from_openssl();
    return std::unexpected(AuthError::SigningFailed);
  }

  return PkeyPtr(pkey, &EVP_PKEY_free);
}

std::expected<std::string, AuthError> sign_rsa_pss(EVP_PKEY* pkey,
                                                   std::string_view message) {
  MdCtxPtr ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
  if (!ctx) {
    set_last_sign_error_from_openssl();
    return std::unexpected(AuthError::SigningFailed);
  }

  EVP_PKEY_CTX* pkey_ctx = nullptr;
  if (EVP_DigestSignInit(ctx.get(), &pkey_ctx, EVP_sha256(), nullptr, pkey) <= 0) {
    set_last_sign_error_from_openssl();
    return std::unexpected(AuthError::SigningFailed);
  }

  if (EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) <= 0) {
    set_last_sign_error_from_openssl();
    return std::unexpected(AuthError::SigningFailed);
  }

  if (EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, -1) <= 0) {
    set_last_sign_error_from_openssl();
    return std::unexpected(AuthError::SigningFailed);
  }

  if (EVP_DigestSignUpdate(ctx.get(), message.data(), message.size()) <= 0) {
    set_last_sign_error_from_openssl();
    return std::unexpected(AuthError::SigningFailed);
  }

  size_t sig_len = 0;
  if (EVP_DigestSignFinal(ctx.get(), nullptr, &sig_len) <= 0) {
    set_last_sign_error_from_openssl();
    return std::unexpected(AuthError::SigningFailed);
  }

  std::string signature(sig_len, '\0');
  if (EVP_DigestSignFinal(ctx.get(),
                          reinterpret_cast<unsigned char*>(signature.data()),
                          &sig_len) <= 0) {
    set_last_sign_error_from_openssl();
    return std::unexpected(AuthError::SigningFailed);
  }
  signature.resize(sig_len);

  return signature;
}

std::expected<std::string, AuthError> base64_encode(const std::string& bytes) {
  std::string b64;
  b64.resize(4 * ((bytes.size() + 2) / 3));
  int out_len = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(b64.data()),
                                reinterpret_cast<const unsigned char*>(bytes.data()),
                                static_cast<int>(bytes.size()));
  if (out_len <= 0) {
    set_last_sign_error_from_openssl();
    return std::unexpected(AuthError::SigningFailed);
  }
  b64.resize(static_cast<size_t>(out_len));
  return b64;
}

} // namespace

const char* to_string(AuthError error) {
  switch (error) {
    case AuthError::MissingKeyId:
      return "missing key id";
    case AuthError::MissingPrivateKey:
      return "missing private key";
    case AuthError::SigningFailed:
      return "signing failed";
  }
  return "unknown auth error";
}

const char* last_sign_error() {
  return g_last_sign_error.empty() ? "no error detail" : g_last_sign_error.c_str();
}

std::expected<AuthConfig, AuthError> load_auth_from_env() {
  const char* key_id = std::getenv("KALSHI_API_KEY");
  const char* key_pem = std::getenv("KALSHI_PRIVATE_KEY");
  const char* key_path = std::getenv("KALSHI_PRIVATE_KEY_PATH");

  if (!key_id) {
    return std::unexpected(AuthError::MissingKeyId);
  }

  AuthConfig config;
  config.key_id = key_id;

  if (key_pem) {
    config.private_key_pem = key_pem;
  } else if (key_path) {
    config.private_key_pem = read_file(key_path);
  }

  if (config.private_key_pem.empty()) {
    return std::unexpected(AuthError::MissingPrivateKey);
  }

  return config;
}

std::expected<std::string, AuthError> sign_ws_message(std::string_view private_key_pem,
                                                      std::string_view message) {
  ensure_legacy_provider_loaded();
  if (is_openssh_key(private_key_pem)) {
    g_last_sign_error =
      "OpenSSH private key format detected; convert to PEM (PKCS#8) for OpenSSL";
    return std::unexpected(AuthError::SigningFailed);
  }

  auto pkey = load_private_key(private_key_pem);
  if (!pkey) {
    return std::unexpected(pkey.error());
  }

  auto signature = sign_rsa_pss(pkey->get(), message);
  if (!signature) {
    return std::unexpected(signature.error());
  }

  return base64_encode(*signature);
}

std::expected<std::vector<Header>, AuthError> build_ws_headers(
  const AuthConfig& auth,
  std::string_view path,
  std::int64_t timestamp_ms) {
  std::string msg = std::to_string(timestamp_ms);
  msg.append("GET");
  msg.append(path);

  auto signature = sign_ws_message(auth.private_key_pem, msg);
  if (!signature) {
    return std::unexpected(signature.error());
  }

  return std::vector<Header>{
    {"KALSHI-ACCESS-KEY", auth.key_id},
    {"KALSHI-ACCESS-SIGNATURE", *signature},
    {"KALSHI-ACCESS-TIMESTAMP", std::to_string(timestamp_ms)}
  };
}

} // namespace kalshi
