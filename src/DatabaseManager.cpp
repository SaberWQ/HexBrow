#include "DatabaseManager.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <functional>

/*
 * УКР: Хешує пароль за допомогою std::hash.
 *      УВАГА: std::hash не призначений для паролів у продакшені —
 *      він нестійкий до brute-force. Для реальних проєктів
 *      використовуйте bcrypt/scrypt/Argon2.
 * ENG: Hashes a password using std::hash.
 *      WARNING: std::hash is NOT for production passwords —
 *      it is vulnerable to brute-force. Use bcrypt/scrypt/Argon2.
 */
std::string hashPassword(const std::string& password) {
    std::hash<std::string> hasher;                    // Initialize hasher
    return std::to_string(hasher(password));          // Convert to string
}

/*
 * УКР: Безпечно витягує текст з SQLite-запиту.
 *      Повертає "" якщо значення NULL.
 * ENG: Safely extracts text from an SQLite query.
 *      Returns "" if the value is NULL.
 */
std::string safeGetText(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col); // Get text pointer
    return text ? reinterpret_cast<const char*>(text) : "";     // NULL check
}

/*
 * УКР: Конструктор. Відкриває з'єднання з SQLite.
 * ENG: Constructor. Opens a SQLite connection.
 */
DatabaseManager::DatabaseManager(const std::string& dbPath) : m_db(nullptr) {
    if (sqlite3_open(dbPath.c_str(), &m_db) != SQLITE_OK) {      // Try to open
        if (m_db) sqlite3_close(m_db);                          // Close on failure
        throw std::runtime_error("DB open error");              // Propagate error
    }
}

/*
 * УКР: Деструктор. Закриває з'єднання (RAII).
 * ENG: Destructor. Closes the connection (RAII).
 */
DatabaseManager::~DatabaseManager() {
    if (m_db) sqlite3_close(m_db);                              // Free resource
}

/*
 * УКР: Ініціалізація схеми БД. Перевіряє наявність колонки email
 *      у таблиці users. Якщо її немає — видаляє стару таблицю
 *      (спрощена міграція) і створює нову.
 * ENG: DB schema init. Checks if users table has email column.
 *      If not — drops the old table (simplified migration)
 *      and creates a new one.
 */
void DatabaseManager::initialize() {
    bool needMigration = false;                                 // Migration flag
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, "PRAGMA table_info(users);", -1, &stmt, nullptr) == SQLITE_OK) {
        bool hasEmail = false;
        while (sqlite3_step(stmt) == SQLITE_ROW) {               // Iterate columns
            std::string colName = safeGetText(stmt, 1);
            if (colName == "email") hasEmail = true;            // Found email column
        }
        sqlite3_finalize(stmt);
        if (!hasEmail) needMigration = true;                    // Old schema
    }

    if (needMigration) {
        std::cout << "WARNING: old DB schema detected. Dropping users table." << std::endl;
        sqlite3_exec(m_db, "DROP TABLE IF EXISTS users;", nullptr, nullptr, nullptr); // Drop old
    }

    const char* sqlCreate = 
        "CREATE TABLE IF NOT EXISTS products ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT, code TEXT, manufacturer TEXT, description TEXT, "
        "imageUrl TEXT, price TEXT, link TEXT, owner_id INTEGER DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT, email TEXT UNIQUE, password TEXT, "
        "avatar TEXT, description TEXT);";
    sqlite3_exec(m_db, sqlCreate, nullptr, nullptr, nullptr);   // Create tables
}

/*
 * УКР: Додає новий товар у БД.
 *      Використовує параметризований запит (захист від SQL-ін'єкцій).
 * ENG: Inserts a new product into the DB.
 *      Uses a parameterized query (SQL injection protection).
 */
bool DatabaseManager::addProduct(const Product& product) {
    const char* sql = "INSERT INTO products (name, code, manufacturer, description, imageUrl, price, link, owner_id) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false; // Prepare error

    sqlite3_bind_text(stmt, 1, product.name.c_str(), -1, SQLITE_TRANSIENT);         // Bind name
    sqlite3_bind_text(stmt, 2, product.code.c_str(), -1, SQLITE_TRANSIENT);         // Bind code
    sqlite3_bind_text(stmt, 3, product.manufacturer.c_str(), -1, SQLITE_TRANSIENT); // Bind manufacturer
    sqlite3_bind_text(stmt, 4, product.description.c_str(), -1, SQLITE_TRANSIENT);  // Bind description
    sqlite3_bind_text(stmt, 5, product.imageUrl.c_str(), -1, SQLITE_TRANSIENT);     // Bind image URL
    sqlite3_bind_text(stmt, 6, product.price.c_str(), -1, SQLITE_TRANSIENT);        // Bind price
    sqlite3_bind_text(stmt, 7, product.link.c_str(), -1, SQLITE_TRANSIENT);         // Bind link
    sqlite3_bind_int(stmt, 8, product.owner_id);                                    // Bind owner ID

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);         // Execute statement
    sqlite3_finalize(stmt);                                     // Free statement
    return success;
}

/*
 * УКР: Оновлює існуючий товар за id.
 * ENG: Updates an existing product by id.
 */
bool DatabaseManager::updateProduct(const Product& product) {
    const char* sql = "UPDATE products SET name=?, code=?, manufacturer=?, description=?, "
                      "imageUrl=?, price=?, link=? WHERE id=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, product.name.c_str(), -1, SQLITE_TRANSIENT);         // Bind name
    sqlite3_bind_text(stmt, 2, product.code.c_str(), -1, SQLITE_TRANSIENT);         // Bind code
    sqlite3_bind_text(stmt, 3, product.manufacturer.c_str(), -1, SQLITE_TRANSIENT); // Bind manufacturer
    sqlite3_bind_text(stmt, 4, product.description.c_str(), -1, SQLITE_TRANSIENT);  // Bind description
    sqlite3_bind_text(stmt, 5, product.imageUrl.c_str(), -1, SQLITE_TRANSIENT);     // Bind image URL
    sqlite3_bind_text(stmt, 6, product.price.c_str(), -1, SQLITE_TRANSIENT);        // Bind price
    sqlite3_bind_text(stmt, 7, product.link.c_str(), -1, SQLITE_TRANSIENT);         // Bind link
    sqlite3_bind_int(stmt, 8, product.id);                                          // Bind product ID

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);         // Execute
    sqlite3_finalize(stmt);
    return success;
}

/*
 * УКР: Видаляє товар за id.
 * ENG: Deletes a product by id.
 */
bool DatabaseManager::deleteProduct(int id) {
    const char* sql = "DELETE FROM products WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);                              // Bind ID
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

/*
 * УКР: Повертає всі товари (новіші — першими).
 * ENG: Returns all products (newest first).
 */
std::vector<Product> DatabaseManager::getAllProducts() const {
    std::vector<Product> results;
    const char* sql = "SELECT id, name, code, manufacturer, description, imageUrl, price, link, owner_id "
                      "FROM products ORDER BY id DESC;";        // DESC = newest first
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {               // Iterate rows
            Product p;
            p.id = sqlite3_column_int(stmt, 0);                 // Column 0
            p.name = safeGetText(stmt, 1);                      // Column 1
            p.code = safeGetText(stmt, 2);                      // Column 2
            p.manufacturer = safeGetText(stmt, 3);              // Column 3
            p.description = safeGetText(stmt, 4);               // Column 4
            p.imageUrl = safeGetText(stmt, 5);                  // Column 5
            p.price = safeGetText(stmt, 6);                     // Column 6
            p.link = safeGetText(stmt, 7);                      // Column 7
            p.owner_id = sqlite3_column_int(stmt, 8);           // Column 8
            results.push_back(p);                               // Append
        }
        sqlite3_finalize(stmt);
    }
    return results;
}

/*
 * УКР: Пошук товарів за назвою або виробником (частковий збіг).
 * ENG: Searches products by name or manufacturer (partial match).
 */
std::vector<Product> DatabaseManager::searchProducts(const std::string& query) const {
    std::vector<Product> results;
    std::string sql = "SELECT id, name, code, manufacturer, description, imageUrl, price, link, owner_id FROM products";
    if (!query.empty()) {
        sql += " WHERE name LIKE ? OR manufacturer LIKE ?";     // Search condition
    }
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return results;

    if (!query.empty()) {
        std::string likeQuery = "%" + query + "%";              // Add wildcards
        sqlite3_bind_text(stmt, 1, likeQuery.c_str(), -1, SQLITE_TRANSIENT); // Bind 1
        sqlite3_bind_text(stmt, 2, likeQuery.c_str(), -1, SQLITE_TRANSIENT); // Bind 2
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

/*
 * УКР: Повертає товар за id або порожню структуру.
 * ENG: Returns product by id or empty struct.
 */
Product DatabaseManager::getProductById(int id) const {
    Product p = {0, "", "", "", "", "", "", "", 0, 0};          // Empty product
    const char* sql = "SELECT id, name, code, manufacturer, description, imageUrl, price, link, owner_id "
                      "FROM products WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {                 // Found
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

/*
 * УКР: Реєструє нового користувача (пароль хешується).
 * ENG: Registers a new user (password is hashed).
 */
bool DatabaseManager::registerUser(const std::string& nickname, const std::string& email, const std::string& password) {
    const char* sql = "INSERT INTO users (username, email, password) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, nickname.c_str(), -1, SQLITE_TRANSIENT);              // Bind nickname
    sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);                 // Bind email
    sqlite3_bind_text(stmt, 3, hashPassword(password).c_str(), -1, SQLITE_TRANSIENT); // Bind hash

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

/*
 * УКР: Перевіряє email та хеш пароля.
 * ENG: Verifies email and password hash.
 */
bool DatabaseManager::loginUser(const std::string& email, const std::string& password) {
    const char* sql = "SELECT id FROM users WHERE email = ? AND password = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);                 // Bind email
    sqlite3_bind_text(stmt, 2, hashPassword(password).c_str(), -1, SQLITE_TRANSIENT); // Bind hash

    bool success = (sqlite3_step(stmt) == SQLITE_ROW);          // Row exists = success
    sqlite3_finalize(stmt);
    return success;
}

/*
 * УКР: Повертає профіль користувача за email.
 * ENG: Returns user profile by email.
 */
User DatabaseManager::getUserProfile(const std::string& email) const {
    User u = {0, "", email, "", "", ""};                        // Empty user
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

/*
 * УКР: Оновлює нікнейм, аватар та опис профілю.
 * ENG: Updates nickname, avatar, and description.
 */
bool DatabaseManager::updateUserProfile(const std::string& email, const std::string& nickname, const std::string& avatar, const std::string& desc) {
    const char* sql = "UPDATE users SET username = ?, avatar = ?, description = ? WHERE email = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, nickname.c_str(), -1, SQLITE_TRANSIENT);              // Bind nickname
    sqlite3_bind_text(stmt, 2, avatar.c_str(), -1, SQLITE_TRANSIENT);                // Bind avatar
    sqlite3_bind_text(stmt, 3, desc.c_str(), -1, SQLITE_TRANSIENT);                  // Bind description
    sqlite3_bind_text(stmt, 4, email.c_str(), -1, SQLITE_TRANSIENT);                 // Bind email

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

/*
 * УКР: Повертає ID користувача за email (0 якщо не знайдено).
 * ENG: Returns user ID by email (0 if not found).
 */
int DatabaseManager::getUserId(const std::string& email) const {
    int id = 0;
    const char* sql = "SELECT id FROM users WHERE email = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int(stmt, 0); // Extract ID
        sqlite3_finalize(stmt);
    }
    return id;
}

/*
 * УКР: Повертає нікнейм користувача за email.
 * ENG: Returns user nickname by email.
 */
std::string DatabaseManager::getNickname(const std::string& email) const {
    std::string nickname = "";
    const char* sql = "SELECT username FROM users WHERE email = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) nickname = safeGetText(stmt, 0); // Extract nickname
        sqlite3_finalize(stmt);
    }
    return nickname;
}