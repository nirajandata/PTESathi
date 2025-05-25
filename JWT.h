#pragma once

#include <crow.h>
#include <jwt-cpp/jwt.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <regex>
#include <string>

namespace auth_utils {
std::string generate_salt(size_t length = 16) {
  unsigned char buffer[length];
  RAND_bytes(buffer, length);

  std::string result;
  for (size_t i = 0; i < length; i++) {
    char hex[3];
    sprintf(hex, "%02x", buffer[i]);
    result += hex;
  }
  return result;
}

// Hash password with salt using modern EVP API (OpenSSL 3.0 compatible)
std::string hash_password(const std::string &password,
                          const std::string &salt) {
  std::string salted = password + salt;

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;

  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
  EVP_DigestUpdate(ctx, salted.c_str(), salted.size());
  EVP_DigestFinal_ex(ctx, hash, &hash_len);
  EVP_MD_CTX_free(ctx);

  std::string result;
  for (unsigned int i = 0; i < hash_len; i++) {
    char hex[3];
    sprintf(hex, "%02x", hash[i]);
    result += hex;
  }
  return result;
}

bool is_valid_email(const std::string &email) {
  const std::regex pattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
  return std::regex_match(email, pattern);
}

bool is_valid_username(const std::string &username) {
  // Username should be alphanumeric and at least 3 characters
  const std::regex pattern(R"([a-zA-Z0-9_]{3,})");
  return std::regex_match(username, pattern);
}

bool is_strong_password(const std::string &password) {
  // Password should be at least 8 characters
  if (password.length() < 8) {
    return false;
  }

  // Check for at least one uppercase letter, one lowercase letter, and one
  // digit
  bool has_upper = false, has_lower = false, has_digit = false;
  for (char c : password) {
    if (isupper(c))
      has_upper = true;
    if (islower(c))
      has_lower = true;
    if (isdigit(c))
      has_digit = true;
  }

  return has_upper && has_lower && has_digit;
}
} // namespace auth_utils

class Config {
public:
  static Config &getInstance() {
    static Config instance;
    return instance;
  }

  std::string getJwtSecretKey() const {
    char *env_secret = std::getenv("JWT_SECRET_KEY");
    if (env_secret != nullptr) {
      return std::string(env_secret);
    }

    return "this_is_a_development_key_replace_in_production_0123456789";
  }

  int getJwtExpirationHours() const { return 24; }

private:
  Config() {}
  Config(const Config &) = delete;
  Config &operator=(const Config &) = delete;
};

class JsonResponse {
public:
  static crow::response success(int status_code, const std::string &message) {
    crow::json::wvalue response;
    response["status"] = "success";
    response["message"] = message;
    return crow::response(status_code, response);
  }

  template <typename T>
  static crow::response success(int status_code, T &&data) {
    crow::json::wvalue response;
    response["status"] = "success";
    response["data"] = std::move(data);
    return crow::response(status_code, response);
  }

  static crow::response error(int status_code, const std::string &message) {
    crow::json::wvalue response;
    response["status"] = "error";
    response["message"] = message;
    return crow::response(status_code, response);
  }
};

User validate_jwt(const std::string &token, Database &db) {
  std::string secret_key = Config::getInstance().getJwtSecretKey();

  auto decoded = jwt::decode(token);
  auto verifier = jwt::verify()
                      .allow_algorithm(jwt::algorithm::hs256{secret_key})
                      .with_issuer("auth_service");

  verifier.verify(decoded);

  if (!decoded.has_payload_claim("username")) {
    throw std::runtime_error("Token missing username claim");
  }

  auto username = decoded.get_payload_claim("username").as_string();
  return db.getUser(username);
}
