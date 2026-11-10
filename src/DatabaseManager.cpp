#include "DatabaseManager.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>

// Правильна функція для кирилиці
std::string toLowerUtf8(const std::string& s) {
    std::string r = s;
    for (size_t i = 0; i < r.size(); ++i) {
        unsigned char c = r[i];
        if (c >= 'A' && c <= 'Z') { r[i] = c + 32; continue; }
        if (c == 0xD0 && i + 1 < r.size()) {
            unsigned char c2 = r[i + 1];
            if (c2 >= 0x90 && c2 <= 0x9F)      { r[i + 1] = c2 + 0x20; }
            else if (c2 >= 0xA0 && c2 <= 0xAF) { r[i] = 0xD1; r[i + 1] = c2 - 0x20; }
            else if (c2 == 0x86)               { r[i] = 0xD1; r[i + 1] = 0x96; }
            else if (c2 == 0x87)               { r[i] = 0xD1; r[i + 1] = 0x97; }
            else if (c2 == 0x84)               { r[i] = 0xD1; r[i + 1] = 0x94; }
            ++i;
        } else if (c == 0xD2 && i + 1 < r.size() && (unsigned char)r[i + 1] == 0x90) {
            r[i + 1] = 0x91; ++i;
        }
    }
    return r;
}

// Екранування символів % та _ для SQL LIKE
std::string escapeLike(const std::string& input) {
    std::string result;
    for (char c : input) {
        if (c == '%' || c == '_' || c == '\\') result += '\\';
        result += c;
    }
    return result;
}

static void lower_utf8_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc == 1) {
        const unsigned char *text = sqlite3_value_text(argv[0]);
        if (text) {
            std::string str = reinterpret_cast<const char*>(text);
            std::string lower = toLowerUtf8(str);
            sqlite3_result_text(context, lower.c_str(), -1, SQLITE_TRANSIENT);
            return;
        }
    }
    sqlite3_result_null(context);
}

std::string safeGetText(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

DatabaseManager::DatabaseManager(const std::string& dbPath) : m_db(nullptr) {
    if (sqlite3_open(dbPath.c_str(), &m_db) != SQLITE_OK) {
        std::string err = m_db ? sqlite3_errmsg(m_db) : "Unknown error";
        if (m_db) sqlite3_close(m_db);
        throw std::runtime_error("Не вдалося відкрити базу даних: " + err);
    }
    sqlite3_create_function(m_db, "lower_utf8", 1, SQLITE_UTF8, NULL, &lower_utf8_func, NULL, NULL);
}

DatabaseManager::~DatabaseManager() {
    if (m_db) sqlite3_close(m_db);
}

void DatabaseManager::migrateDatabase() {
    const char* columnsToAdd[] = {"imageUrl TEXT", "price TEXT", "link TEXT", "isFavorite INTEGER DEFAULT 0"};
    for (const char* colDef : columnsToAdd) {
        std::string sql = "ALTER TABLE products ADD COLUMN " + std::string(colDef) + ";";
        sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, nullptr);
    }
}

void DatabaseManager::initialize() {
    const char* sqlCreate = 
        "CREATE TABLE IF NOT EXISTS products (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, code TEXT NOT NULL, manufacturer TEXT NOT NULL, description TEXT);"
        "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE NOT NULL, password TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS history (id INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT, title TEXT, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";

    char* errMsg = nullptr;
    if (sqlite3_exec(m_db, sqlCreate, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error = errMsg ? errMsg : "Unknown";
        sqlite3_free(errMsg);
        throw std::runtime_error("Помилка ініціалізації БД: " + error);
    }

    migrateDatabase();

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, "SELECT COUNT(*) FROM products;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 0) {
            const char* sqlInsert = 
                "INSERT INTO products (name, code, manufacturer, description, imageUrl, price, link, isFavorite) VALUES "
                "('Ноутбук Dell XPS 13', 'DXPS13-001', 'Dell', '13-дюймовий ультрабук', 'https://dummyjson.com/image/400x400?text=Dell+XPS', '1200', 'https://www.google.com/search?q=Dell+XPS+13&tbm=shop', 0),"
                "('Миша Logitech MX Master 3', 'LMX3-002', 'Logitech', 'Бездротова миша', 'https://dummyjson.com/image/400x400?text=Logitech+MX', '99', 'https://www.google.com/search?q=Logitech+MX+Master+3&tbm=shop', 0),"
                "('Клавіатура Apple Magic Keyboard', 'AMK-003', 'Apple', 'Бездротова клавіатура', 'https://dummyjson.com/image/400x400?text=Apple+Keyboard', '150', 'https://www.google.com/search?q=Apple+Magic+Keyboard&tbm=shop', 0);";
            sqlite3_exec(m_db, sqlInsert, nullptr, nullptr, nullptr);
        }
        sqlite3_finalize(stmt);
    }
}

bool DatabaseManager::addProduct(const Product& product) {
    const char* checkSql = "SELECT id FROM products WHERE code = ?;";
    sqlite3_stmt* checkStmt;
    if (sqlite3_prepare_v2(m_db, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(checkStmt, 1, product.code.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            sqlite3_finalize(checkStmt);
            return false; 
        }
        sqlite3_finalize(checkStmt);
    }

    const char* sql = "INSERT INTO products (name, code, manufacturer, description, imageUrl, price, link, isFavorite) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, product.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, product.code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, product.manufacturer.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, product.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, product.imageUrl.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, product.price.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, product.link.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, product.isFavorite);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::toggleFavorite(int id, int currentStatus) {
    const char* sql = "UPDATE products SET isFavorite = ? WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, currentStatus == 1 ? 0 : 1);
    sqlite3_bind_int(stmt, 2, id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Product> DatabaseManager::getFavorites() const {
    std::vector<Product> results;
    const char* sql = "SELECT id, name, code, manufacturer, description, imageUrl, price, link, isFavorite FROM products WHERE isFavorite = 1;";
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
            p.isFavorite = sqlite3_column_int(stmt, 8);
            results.push_back(p);
        }
        sqlite3_finalize(stmt);
    }
    return results;
}

std::vector<Product> DatabaseManager::searchProducts(const std::string& query) const {
    std::vector<Product> results;
    std::string sql = "SELECT id, name, code, manufacturer, description, imageUrl, price, link, isFavorite FROM products";
    if (!query.empty()) {
        sql += " WHERE lower_utf8(name) LIKE lower_utf8(?) ESCAPE '\\' OR lower_utf8(manufacturer) LIKE lower_utf8(?) ESCAPE '\\' OR lower_utf8(code) LIKE lower_utf8(?) ESCAPE '\\'";
    }

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }

    if (!query.empty()) {
        std::string escapedQuery = escapeLike(query);
        std::string likeQuery = "%" + escapedQuery + "%";
        sqlite3_bind_text(stmt, 1, likeQuery.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, likeQuery.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, likeQuery.c_str(), -1, SQLITE_TRANSIENT);
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
        p.isFavorite = sqlite3_column_int(stmt, 8);
        results.push_back(p);
    }
    sqlite3_finalize(stmt);
    return results;
}

bool DatabaseManager::updateProduct(const Product& product) {
    const char* sql = "UPDATE products SET name=?, code=?, manufacturer=?, description=?, imageUrl=?, price=?, link=?, isFavorite=? WHERE id=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, product.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, product.code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, product.manufacturer.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, product.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, product.imageUrl.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, product.price.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, product.link.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, product.isFavorite);
    sqlite3_bind_int(stmt, 9, product.id);
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

Product DatabaseManager::getById(int id) {
    Product p = {0, "", "", "", "", "", "", "", 0};
    const char* sql = "SELECT id, name, code, manufacturer, description, imageUrl, price, link, isFavorite FROM products WHERE id = ?;";
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
            p.isFavorite = sqlite3_column_int(stmt, 8);
        }
        sqlite3_finalize(stmt);
    }
    return p;
}

bool DatabaseManager::registerUser(const std::string& username, const std::string& password) {
    const char* sql = "INSERT INTO users (username, password) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::loginUser(const std::string& username, const std::string& password) {
    const char* sql = "SELECT id FROM users WHERE username = ? AND password = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return success;
}

void DatabaseManager::addHistory(const std::string& url, const std::string& title) {
    const char* sql = "INSERT INTO history (url, title) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}