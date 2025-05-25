#pragma once
#include <sqlite3.h>
#include <stdexcept>
#include <string>

struct User {
  std::string username;
  std::string password_hash;
  std::string salt;
  std::string email;
};

class Database {
public:
  Database(const std::string &path) : db_(nullptr) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }
    initializeSchema();
  }

  ~Database() {
    if (db_)
      sqlite3_close(db_);
  }

  bool userExists(const std::string &username) {
    const char *sql = "SELECT 1 FROM users WHERE username = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
  }

  bool emailExists(const std::string &email) {
    const char *sql = "SELECT 1 FROM users WHERE email = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
  }

  User getUser(const std::string &username) {
    const char *sql = "SELECT username, password_hash, salt, email FROM users "
                      "WHERE username = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
      sqlite3_finalize(stmt);
      throw std::runtime_error("User not found");
    }

    User user;
    user.username =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    user.password_hash =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    user.salt = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    user.email = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));

    sqlite3_finalize(stmt);
    return user;
  }

  void addUser(const User &user) {
    const char *sql = "INSERT INTO users (username, password_hash, salt, "
                      "email) VALUES (?, ?, ?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }

    sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user.password_hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, user.salt.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, user.email.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      std::string error = sqlite3_errmsg(db_);
      sqlite3_finalize(stmt);
      throw std::runtime_error(error);
    }
    sqlite3_finalize(stmt);
  }

private:
  sqlite3 *db_;

  void initializeSchema() {
    const char *sql = R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT UNIQUE NOT NULL,
                password_hash TEXT NOT NULL,
                salt TEXT NOT NULL,
                email TEXT UNIQUE NOT NULL
            );
        )";

    char *errMsg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
      std::string error = errMsg;
      sqlite3_free(errMsg);
      throw std::runtime_error(error);
    }
  }
};
