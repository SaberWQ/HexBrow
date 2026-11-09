#pragma once
#include "Product.h"
#include <sqlite3.h>
#include <vector>
#include <string>

class DatabaseManager {
public:
    explicit DatabaseManager(const std::string& dbPath);
    ~DatabaseManager();
    void initialize();
    std::vector<Product> searchProducts(const std::string& query) const;

private:
    sqlite3* m_db;
};