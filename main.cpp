#include <crow.h>
#include <ctime>
#include <jwt-cpp/jwt.h>
#include <map>
#include <string>

struct User {
  std::string username;
  std::string password;
  std::string email;
};

struct JwtMiddleware : crow::ILocalMiddleware {
  struct context {
    User user;
  };

  std::map<std::string, User> users;
  std::string secret_key = "your_256_bit_secret";

  void before_handle(crow::request &req, crow::response &res, context &ctx) {
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) {
      res.code = 401;
      res.write("Authorization header missing");
      res.end();
      return;
    }

    try {
      auto token = auth_header.substr(7);
      auto decoded = jwt::decode(token);
      jwt::verify()
          .allow_algorithm(jwt::algorithm::hs256{secret_key})
          .verify(decoded);

      auto username = decoded.get_payload_claim("username").as_string();
      if (users.find(username) == users.end()) {
        res.code = 401;
        res.write("Invalid user");
        res.end();
        return;
      }

      ctx.user = users[username];
    } catch (const std::exception &e) {
      res.code = 401;
      res.write(std::string("Invalid token: ") + e.what());
      res.end();
    }
  }

  void after_handle(crow::request &, crow::response &, context &) {}
};

int main() {
  crow::App<JwtMiddleware> app;

  // Capture app by reference in lambdas
  CROW_ROUTE(app, "/auth/signup")
      .methods("POST"_method)([&app](const crow::request &req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password") ||
            !body.has("email")) {
          return crow::response(400, "Missing fields");
        }

        std::string username = body["username"].s();
        auto &users = app.template get_middleware<JwtMiddleware>().users;

        if (users.find(username) != users.end()) {
          return crow::response(409, "User already exists");
        }

        users[username] =
            User{username, body["password"].s(), body["email"].s()};

        return crow::response(201, "User created");
      });

  CROW_ROUTE(app, "/auth/login")
      .methods("POST"_method)([&app](const crow::request &req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password")) {
          return crow::response(400, "Missing fields");
        }

        auto &users = app.template get_middleware<JwtMiddleware>().users;
        std::string username = body["username"].s();

        if (users.find(username) == users.end() ||
            users[username].password != body["password"].s()) {
          return crow::response(401, "Invalid credentials");
        }

        auto token = jwt::create()
                         .set_issuer("auth0")
                         .set_type("JWS")
                         .set_payload_claim("username", jwt::claim(username))
                         .set_issued_at(std::chrono::system_clock::now())
                         .set_expires_at(std::chrono::system_clock::now() +
                                         std::chrono::hours(1))
                         .sign(jwt::algorithm::hs256{"your_256_bit_secret"});

        crow::json::wvalue response;
        response["token"] = token;
        return crow::response{response};
      });

  CROW_ROUTE(app, "/auth/me")
      .methods("GET"_method)
      .CROW_MIDDLEWARES(app, JwtMiddleware)([&app](const crow::request &req) {
        auto &ctx = app.template get_context<JwtMiddleware>(req);
        crow::json::wvalue response;
        response["username"] = ctx.user.username;
        response["email"] = ctx.user.email;
        return crow::response{response};
      });

  app.port(8080).multithreaded().run();
}
