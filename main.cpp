#include <crow.h>
#include <ctime>
#include <jwt-cpp/jwt.h>
#include <map>
#include <string>

struct User {
  std::string username;
  std::string password; // In real-world, store hashed password
  std::string email;
};

// In-memory database (replace with real database in production)
std::map<std::string, User> users;
std::string secret_key =
    "your_256_bit_secret"; // Use strong secret in production

struct JwtMiddleware {
  struct context {
    User user;
  };

  void before_handle(crow::request &req, crow::response &res, context &ctx) {
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) {
      res.code = 401;
      res.end("Authorization header missing");
      return;
    }

    auto token = auth_header.substr(7); // Remove "Bearer "
    try {
      auto decoded = jwt::decode(token);
      jwt::verify()
          .allow_algorithm(jwt::algorithm::hs256{secret_key})
          .verify(decoded);

      auto username = decoded.get_payload_claim("username").as_string();
      if (users.find(username) == users.end()) {
        res.code = 401;
        res.end("Invalid user");
        return;
      }

      ctx.user = users[username];
    } catch (const std::exception &e) {
      res.code = 401;
      res.end("Invalid token: " + std::string(e.what()));
      return;
    }
  }

  void after_handle(crow::request &req, crow::response &res, context &ctx) {}
};

int main() {
  crow::App<JwtMiddleware> app;

  // Signup route
  CROW_ROUTE(app, "/auth/signup")
      .methods("POST"_method)([](const crow::request &req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password") ||
            !body.has("email")) {
          return crow::response(400, "Missing fields");
        }

        std::string username = body["username"].s();
        if (users.find(username) != users.end()) {
          return crow::response(409, "User already exists");
        }

        User new_user{username, body["password"].s(), body["email"].s()};
        users[username] = new_user;

        return crow::response(201, "User created");
      });

  // Login route
  CROW_ROUTE(app, "/auth/login")
      .methods("POST"_method)([](const crow::request &req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password")) {
          return crow::response(400, "Missing fields");
        }

        std::string username = body["username"].s();
        if (users.find(username) == users.end()) {
          return crow::response(401, "Invalid credentials");
        }

        const User &user = users[username];
        if (user.password != body["password"].s()) {
          return crow::response(401, "Invalid credentials");
        }

        auto token = jwt::create()
                         .set_issuer("auth0")
                         .set_type("JWS")
                         .set_payload_claim("username", jwt::claim(username))
                         .set_issued_at(std::chrono::system_clock::now())
                         .set_expires_at(std::chrono::system_clock::now() +
                                         std::chrono::hours{1})
                         .sign(jwt::algorithm::hs256{secret_key});

        crow::json::wvalue response;
        response["token"] = token;
        return crow::response{response};
      });

  // Get user info route
  CROW_ROUTE(app, "/auth/me")
      .methods("GET"_method)
      .CROW_MIDDLEWARES(app, JwtMiddleware)([&app](const crow::request &req) {
        auto &ctx = app.get_context<JwtMiddleware>(req);
        crow::json::wvalue response;
        response["username"] = ctx.user.username;
        response["email"] = ctx.user.email;
        return crow::response{response};
      });

  app.port(8080).multithreaded().run();
}
