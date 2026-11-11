#include "DatabaseManager.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <functional>

std::string hashPassword(const std::string& password) {
    std::hash<std::string> hasher;
    return std::to_string(hasher(password));
}

std::string safeGetText(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

DatabaseManager::DatabaseManager(const std::string& dbPath) : m_db(nullptr) {
    if (sqlite3_open(dbPath.c_str(), &m_db) != SQLITE_OK) {
        if (m_db) sqlite3_close(m_db);
        throw std::runtime_error("DB open error");
    }
}

DatabaseManager::~DatabaseManager() {
    if (m_db) sqlite3_close(m_db);
}

void DatabaseManager::initialize() {
    bool needMigration = false;
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, "PRAGMA table_info(users);", -1, &stmt, nullptr) == SQLITE_OK) {
        bool hasEmail = false;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string colName = safeGetText(stmt, 1);
            if (colName == "email") hasEmail = true;
        }
        sqlite3_finalize(stmt);
        if (!hasEmail) needMigration = true; 
    }

    if (needMigration) {
        std::cout << "УВАГА: Виявлено стару схему БД. Видаляємо таблицю users для оновлення." << std::endl;
        sqlite3_exec(m_db, "DROP TABLE IF EXISTS users;", nullptr, nullptr, nullptr);
    }

    const char* sqlCreate = 
        "CREATE TABLE IF NOT EXISTS products (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, code TEXT, manufacturer TEXT, description TEXT, imageUrl TEXT, price TEXT, link TEXT, owner_id INTEGER DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT, email TEXT UNIQUE, password TEXT, avatar TEXT, description TEXT);";
    sqlite3_exec(m_db, sqlCreate, nullptr, nullptr, nullptr);
}

bool DatabaseManager::addProduct(const Product& product) {
    const char* sql = "INSERT INTO products (name, code, manufacturer, description, imageUrl, price, link, owner_id) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, product.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, product.code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, product.manufacturer.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, product.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, product.imageUrl.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, product.price.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, product.link.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, product.owner_id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::updateProduct(const Product& product) {
    const char* sql = "UPDATE products SET name=?, code=?, manufacturer=?, description=?, imageUrl=?, price=?, link=? WHERE id=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, product.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, product.code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, product.manufacturer.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, product.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, product.imageUrl.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, product.price.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, product.link.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, product.id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::deleteProduct(int id) {
    const char* sql = "DELETE FROM products WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Product> DatabaseManager::getAllProducts() const {
    std::vector<Product> results;
    const char* sql = "SELECT id, name, code, manufacturer, description, imageUrl, price, link, owner_id FROM products ORDER BY id DESC;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Product p;
            p.id = sqlite3_column_int(stmt, 0);
            p.name = safeGetText(stmt, 1);
            p.code = safeGetText(stmt, 2);
            p.manufacturer = safeGetText(stmt, 3);
            p.description = safeGetText(stmt, 4);
            p.imageUrl = safeGetText(stmt, 5);
            p.price = safeGetText(stmt, 6);
            p.link = safeGetText(stmt, 7);
            p.owner_id = sqlite3_column_int(stmt, 8);
            results.push_back(p);
        }
        sqlite3_finalize(stmt);
    }
    return results;
}

std::vector<Product> DatabaseManager::searchProducts(const std::string& query) const {
    std::vector<Product> results;
    std::string sql = "SELECT id, name, code, manufacturer, description, imageUrl, price, link, owner_id FROM products";
    if (!query.empty()) {
        sql += " WHERE name LIKE ? OR manufacturer LIKE ?";
    }
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return results;
    
    if (!query.empty()) {
        std::string likeQuery = "%" + query + "%";
        sqlite3_bind_text(stmt, 1, likeQuery.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, likeQuery.c_str(), -1, SQLITE_TRANSIENT);
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Product p;
        p.id = sqlite3_column_int(stmt, 0);
        p.name = safeGetText(stmt, 1);
        p.code = safeGetText(stmt, 2);
        p.manufacturer = safeGetText(stmt, 3);
        p.description = safeGetText(stmt, 4);
        p.imageUrl = safeGetText(stmt, 5);
        p.price = safeGetText(stmt, 6);
        p.link = safeGetText(stmt, 7);
        p.owner_id = sqlite3_column_int(stmt, 8);
        results.push_back(p);
    }
    sqlite3_finalize(stmt);
    return results;
}

Product DatabaseManager::getProductById(int id) const {
    Product p = {0, "", "", "", "", "", "", "", 0, 0};
    const char* sql = "SELECT id, name, code, manufacturer, description, imageUrl, price, link, owner_id FROM products WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            p.id = sqlite3_column_int(stmt, 0);
            p.name = safeGetText(stmt, 1);
            p.code = safeGetText(stmt, 2);
            p.manufacturer = safeGetText(stmt, 3);
            p.description = safeGetText(stmt, 4);
            p.imageUrl = safeGetText(stmt, 5);
            p.price = safeGetText(stmt, 6);
            p.link = safeGetText(stmt, 7);
            p.owner_id = sqlite3_column_int(stmt, 8);
        }
        sqlite3_finalize(stmt);
    }
    return p;
}

bool DatabaseManager::registerUser(const std::string& nickname, const std::string& email, const std::string& password) {
    const char* sql = "INSERT INTO users (username, email, password) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, nickname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, hashPassword(password).c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::loginUser(const std::string& email, const std::string& password) {
    const char* sql = "SELECT id FROM users WHERE email = ? AND password = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hashPassword(password).c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return success;
}

User DatabaseManager::getUserProfile(const std::string& email) const {
    User u = {0, "", email, "", "", ""};
    const char* sql = "SELECT id, username, avatar, description FROM users WHERE email = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            u.id = sqlite3_column_int(stmt, 0);
            u.username = safeGetText(stmt, 1);
            u.avatar = safeGetText(stmt, 2);
            u.description = safeGetText(stmt, 3);
        }
        sqlite3_finalize(stmt);
    }
    return u;
}

bool DatabaseManager::updateUserProfile(const std::string& email, const std::string& nickname, const std::string& avatar, const std::string& desc) {
    const char* sql = "UPDATE users SET username = ?, avatar = ?, description = ? WHERE email = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, nickname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, avatar.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, desc.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, email.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

int DatabaseManager::getUserId(const std::string& email) const {
    int id = 0;
    const char* sql = "SELECT id FROM users WHERE email = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return id;
}

std::string DatabaseManager::getNickname(const std::string& email) const {
    std::string nickname = "";
    const char* sql = "SELECT username FROM users WHERE email = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) nickname = safeGetText(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return nickname;
}