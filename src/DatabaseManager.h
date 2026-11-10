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
    
    // CRUD
    bool addProduct(const Product& product);
    bool updateProduct(const Product& product);
    bool deleteProduct(int id);
    Product getById(int id);
    std::vector<Product> searchProducts(const std::string& query) const;
    std::vector<Product> getFavorites() const; // Нове
    bool toggleFavorite(int id, int currentStatus); // Нове

    // Користувачі та історія
    bool registerUser(const std::string& username, const std::string& password);
    bool loginUser(const std::string& username, const std::string& password);
    void addHistory(const std::string& url, const std::string& title);

private:
    sqlite3* m_db;
    void migrateDatabase();
};