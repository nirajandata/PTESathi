#include <crow.h>
#include <cstring>
#include <jwt-cpp/jwt.h>
#include <sqlite3.h>

#include "Database.h"
#include "JWT.h"

int main() {
  crow::App app;

  Database db{"auth.db"};

  CROW_ROUTE(app, "/auth/signup")
      .methods("POST"_method)([&db](const crow::request &req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password") ||
            !body.has("email")) {
          return JsonResponse::error(
              400, "Missing required fields: username, password, and email");
        }

        try {
          std::string username = body["username"].s();
          std::string password = body["password"].s();
          std::string email = body["email"].s();

          if (!auth_utils::is_valid_username(username)) {
            return JsonResponse::error(400, "Invalid username format");
          }

          if (!auth_utils::is_valid_email(email)) {
            return JsonResponse::error(400, "Invalid email format");
          }

          if (!auth_utils::is_strong_password(password)) {
            return JsonResponse::error(
                400, "Password does not meet security requirements");
          }

          if (db.userExists(username)) {
            return JsonResponse::error(409, "Username already exists");
          }

          if (db.emailExists(email)) {
            return JsonResponse::error(409, "Email already exists");
          }

          std::string salt = auth_utils::generate_salt();
          std::string password_hash = auth_utils::hash_password(password, salt);

          User new_user{username, password_hash, salt, email};
          db.addUser(new_user);

          return JsonResponse::success(201, "User created successfully");
        } catch (const std::exception &e) {
          return JsonResponse::error(500, std::string("Error: ") + e.what());
        }
      });

  CROW_ROUTE(app, "/auth/login")
      .methods("POST"_method)([&db](const crow::request &req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password")) {
          return JsonResponse::error(400, "Missing username or password");
        }

        try {
          std::string username = body["username"].s();
          std::string password = body["password"].s();

          if (!db.userExists(username)) {
            return JsonResponse::error(401, "Invalid credentials");
          }

          User user = db.getUser(username);
          std::string hashed_password =
              auth_utils::hash_password(password, user.salt);

          if (user.password_hash != hashed_password) {
            return JsonResponse::error(401, "Invalid credentials");
          }

          std::string secret_key = Config::getInstance().getJwtSecretKey();
          int expiration_hours = Config::getInstance().getJwtExpirationHours();

          auto token =
              jwt::create()
                  .set_issuer("auth_service")
                  .set_type("JWS")
                  .set_payload_claim("username", jwt::claim(user.username))
                  .set_issued_at(std::chrono::system_clock::now())
                  .set_expires_at(std::chrono::system_clock::now() +
                                  std::chrono::hours(expiration_hours))
                  .sign(jwt::algorithm::hs256{secret_key});

          crow::json::wvalue response;
          response["token"] = token;
          response["expiresIn"] = expiration_hours * 3600;
          return JsonResponse::success(200, response);
        } catch (const std::exception &e) {
          return JsonResponse::error(401, "Invalid credentials");
        }
      });

  CROW_ROUTE(app, "/auth/me")
      .methods("GET"_method)([&db](const crow::request &req) {
        try {
          auto auth_header = req.get_header_value("Authorization");
          if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
            return JsonResponse::error(
                401, "Missing or invalid authorization header");
          }

          std::string token = auth_header.substr(7);
          User user = validate_jwt(token, db);

          crow::json::wvalue user_data;
          user_data["username"] = user.username;
          user_data["email"] = user.email;

          return JsonResponse::success(200, user_data);
        } catch (const std::exception &e) {
          return JsonResponse::error(
              401, std::string("Authentication failed: ") + e.what());
        }
      });

  CROW_ROUTE(app, "/meow").methods("GET"_method)([]() {
    crow::json::wvalue response;
    response["status"] = "meow meow test";
    return crow::response(response);
  });

  app.port(8080).multithreaded().run();
  return 0;
}
