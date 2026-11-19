#pragma once
#include "Product.h"
#include "User.h"
#include <sqlite3.h>
#include <vector>
#include <string>

/*
 * УКР: Клас-обгортка для роботи з базою даних SQLite.
 *      Інкапсулює всі SQL-запити, використовує параметризовані запити
 *      для захисту від SQL-ін'єкцій.
 * ENG: Wrapper class for SQLite database operations.
 *      Encapsulates all SQL queries, uses parameterized statements
 *      to prevent SQL injection.
 */
class DatabaseManager {
public:
    /*
     * УКР: Відкриває з'єднання з БД за вказаним шляхом.
     *      Кидає std::runtime_error при помилці.
     * ENG: Opens a DB connection at the given path.
     *      Throws std::runtime_error on failure.
     */
    explicit DatabaseManager(const std::string& dbPath);

    /*
     * УКР: Закриває з'єднання (RAII).
     * ENG: Closes the connection (RAII).
     */
    ~DatabaseManager();

    /*
     * УКР: Ініціалізує схему БД та виконує міграцію при потребі.
     * ENG: Initializes the DB schema and migrates if needed.
     */
    void initialize();

    /* ==================== Products CRUD ==================== */
    bool addProduct(const Product& product);                    // Insert new product
    bool updateProduct(const Product& product);                 // Update existing product
    bool deleteProduct(int id);                                 // Delete product by ID
    std::vector<Product> getAllProducts() const;                // Fetch all products
    std::vector<Product> searchProducts(const std::string& query) const; // Search products
    Product getProductById(int id) const;                       // Fetch product by ID

    /* ==================== Users CRUD ==================== */
    bool registerUser(const std::string& nickname, const std::string& email, const std::string& password);
    bool loginUser(const std::string& email, const std::string& password);
    User getUserProfile(const std::string& email) const;
    bool updateUserProfile(const std::string& email, const std::string& nickname, const std::string& avatar, const std::string& desc);
    int getUserId(const std::string& email) const;
    std::string getNickname(const std::string& email) const;

private:
    sqlite3* m_db;             // SQLite connection handle
    void migrateDatabase();    // Internal schema migration
};