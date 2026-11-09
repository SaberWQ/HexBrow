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
    
    // Товари
    std::vector<Product> searchProducts(const std::string& query) const;
    bool addProduct(const Product& product);
    bool deleteProduct(int id);

    // Користувачі
    bool registerUser(const std::string& username, const std::string& password);
    bool loginUser(const std::string& username, const std::string& password);

    // Історія
    void addHistory(const std::string& url, const std::string& title);
    std::vector<std::string> getHistory();

private:
    sqlite3* m_db;
};