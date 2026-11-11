#pragma once
#include "Product.h"
#include "User.h"
#include <sqlite3.h>
#include <vector>
#include <string>

class DatabaseManager {
public:
    explicit DatabaseManager(const std::string& dbPath);
    ~DatabaseManager();
    void initialize();
    
    // Товари
    bool addProduct(const Product& product);
    bool updateProduct(const Product& product);
    bool deleteProduct(int id);
    std::vector<Product> getAllProducts() const;
    std::vector<Product> searchProducts(const std::string& query) const;
    Product getProductById(int id) const;
    
    // Користувачі
    bool registerUser(const std::string& nickname, const std::string& email, const std::string& password);
    bool loginUser(const std::string& email, const std::string& password);
    User getUserProfile(const std::string& email) const;
    bool updateUserProfile(const std::string& email, const std::string& nickname, const std::string& avatar, const std::string& desc);
    int getUserId(const std::string& email) const;
    std::string getNickname(const std::string& email) const;

private:
    sqlite3* m_db;
    void migrateDatabase();
};