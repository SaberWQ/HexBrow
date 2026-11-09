#include "DatabaseManager.h"
#include <iostream>
#include <stdexcept>

DatabaseManager::DatabaseManager(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &m_db) != SQLITE_OK) {
        throw std::runtime_error("Не вдалося відкрити базу даних: " + std::string(sqlite3_errmsg(m_db)));
    }
}

DatabaseManager::~DatabaseManager() {
    if (m_db) {
        sqlite3_close(m_db);
    }
}

void DatabaseManager::initialize() {
    const char* sqlCreate = 
        "CREATE TABLE IF NOT EXISTS products ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "code TEXT NOT NULL,"
        "manufacturer TEXT NOT NULL,"
        "description TEXT);";

    char* errMsg = nullptr;
    if (sqlite3_exec(m_db, sqlCreate, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error = errMsg ? errMsg : "Невідома помилка";
        sqlite3_free(errMsg);
        throw std::runtime_error("Помилка створення таблиці: " + error);
    }

    const char* sqlCount = "SELECT COUNT(*) FROM products;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sqlCount, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            if (count == 0) {
                const char* sqlInsert = 
                    "INSERT INTO products (name, code, manufacturer, description) VALUES "
                    "('Ноутбук Dell XPS 13', 'DXPS13-001', 'Dell', '13-дюймовий ультрабук'),"
                    "('Миша Logitech MX Master 3', 'LMX3-002', 'Logitech', 'Бездротова миша'),"
                    "('Клавіатура Apple Magic Keyboard', 'AMK-003', 'Apple', 'Бездротова клавіатура');";
                sqlite3_exec(m_db, sqlInsert, nullptr, nullptr, nullptr);
            }
        }

    sqlite3_exec(m_db, "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, username TEXT UNIQUE, password TEXT);", 0, 0, 0);
    sqlite3_exec(m_db, "CREATE TABLE IF NOT EXISTS history (id INTEGER PRIMARY KEY, url TEXT, title TEXT, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);", 0, 0, 0);
        sqlite3_finalize(stmt);
    }
}

std::vector<Product> DatabaseManager::searchProducts(const std::string& query) const {
    std::vector<Product> results;
    std::string sql = "SELECT id, name, code, manufacturer, description FROM products";
    if (!query.empty()) {
        sql += " WHERE name LIKE ? OR manufacturer LIKE ? OR code LIKE ?";
    }

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }

    if (!query.empty()) {
        std::string likeQuery = "%" + query + "%";
        sqlite3_bind_text(stmt, 1, likeQuery.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, likeQuery.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, likeQuery.c_str(), -1, SQLITE_STATIC);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Product p;
        p.id = sqlite3_column_int(stmt, 0);
        p.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        p.manufacturer = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        p.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        results.push_back(p);
    }
    sqlite3_finalize(stmt);
    return results;
}


bool DatabaseManager::addProduct(const Product& product) {
    const char* sql = "INSERT INTO products (name, code, manufacturer, description) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, product.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, product.code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, product.manufacturer.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, product.description.c_str(), -1, SQLITE_STATIC);

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


bool DatabaseManager::registerUser(const std::string& username, const std::string& password) {
    // У реальному житті пароль треба хешувати! Для курсової зійде і так.
    const char* sql = "INSERT INTO users (username, password) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::loginUser(const std::string& username, const std::string& password) {
    const char* sql = "SELECT id FROM users WHERE username = ? AND password = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
    bool success = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return success;
}

void DatabaseManager::addHistory(const std::string& url, const std::string& title) {
    const char* sql = "INSERT INTO history (url, title) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}