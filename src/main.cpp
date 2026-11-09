#include <iostream>
#include <string>
#include <cstdlib>
#include <sstream>
#include "DatabaseManager.h"

// Функція для декодування URL (наприклад, %20 -> пробіл)
std::string urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::string hex = str.substr(i + 1, 2);
            std::istringstream(hex) >> std::hex >> value;
            result += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

// Отримання параметра з рядка запиту
std::string getQueryParam(const std::string& query, const std::string& key) {
    size_t pos = query.find(key + "=");
    if (pos == std::string::npos) return "";
    size_t start = pos + key.length() + 1;
    size_t end = query.find('&', start);
    std::string value = query.substr(start, end - start);
    return urlDecode(value);
}

int main() {
    // 1. ОБОВ'ЯЗКОВО: Виводимо HTTP-заголовок
    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";

    // 2. Отримуємо запит
    const char* queryString = std::getenv("QUERY_STRING");
    std::string searchQuery;
    if (queryString) {
        searchQuery = getQueryParam(queryString, "q");
    }

    // 3. Робота з БД
    try {
        // База даних буде створена в поточній директорії
        DatabaseManager db("products.db");
        db.initialize(); // Створить таблицю та додасть тестові дані, якщо потрібно

        auto products = db.searchProducts(searchQuery);

        // 4. Генерація HTML
        std::cout << "<!DOCTYPE html>\n<html lang='uk'>\n<head>\n<meta charset='UTF-8'>\n";
        std::cout << "<title>Результати пошуку</title>\n";
        std::cout << "<style>\n";
        std::cout << "body { font-family: Arial, sans-serif; background-color: #f4f4f4; padding: 20px; }\n";
        std::cout << ".container { max-width: 800px; margin: auto; background: white; padding: 20px; border-radius: 8px; }\n";
        std::cout << ".product { border-bottom: 1px solid #ddd; padding: 10px 0; }\n";
        std::cout << ".product:last-child { border-bottom: none; }\n";
        std::cout << "</style>\n</head>\n<body>\n<div class='container'>\n";
        std::cout << "<h1>Результати пошуку</h1>\n";
        std::cout << "<p>Запит: <strong>" << searchQuery << "</strong></p>\n";
        std::cout << "<p><a href='/index.html'>Повернутися до пошуку</a></p>\n";

        if (products.empty()) {
            std::cout << "<p>Товарів не знайдено.</p>\n";
        } else {
            for (const auto& p : products) {
                std::cout << "<div class='product'>\n";
                std::cout << "<h2>" << p.name << "</h2>\n";
                std::cout << "<p><strong>Код:</strong> " << p.code << "</p>\n";
                std::cout << "<p><strong>Виробник:</strong> " << p.manufacturer << "</p>\n";
                std::cout << "<p><strong>Опис:</strong> " << p.description << "</p>\n";
                std::cout << "</div>\n";
            }
        }
        std::cout << "</div>\n</body>\n</html>\n";

    } catch (const std::exception& e) {
        std::cout << "<html><body><h1>Помилка</h1><p>" << e.what() << "</p></body></html>";
    }

    return 0;
}