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