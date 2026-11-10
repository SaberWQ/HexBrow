#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include "DatabaseManager.h"
#include "HtmlGenerator.h"

// Декодування URL
std::string urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value = 0;
            std::istringstream(str.substr(i + 1, 2)) >> std::hex >> value;
            result += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') result += ' ';
        else result += str[i];
    }
    return result;
}

std::string getQueryParam(const std::string& query, const std::string& key) {
    std::string searchKey = key + "=";
    size_t pos = query.find(searchKey);
    while (pos != std::string::npos) {
        if (pos == 0 || query[pos - 1] == '&') {
            size_t start = pos + searchKey.length();
            size_t end = query.find('&', start);
            return urlDecode(query.substr(start, end == std::string::npos ? std::string::npos : end - start));
        }
        pos = query.find(searchKey, pos + 1);
    }
    return "";
}

int main() {
    const char* queryString = std::getenv("QUERY_STRING");
    std::string searchQuery = queryString ? getQueryParam(queryString, "q") : "";

    try {
        // Для CGI БД має лежати в тій же папці
        DatabaseManager db("products.db");
        db.initialize();
        auto products = db.searchProducts(searchQuery);

        // Генеруємо HTML через наш спільний клас
        std::string title = "Результати пошуку: " + searchQuery;
        std::string html = HtmlGenerator::generatePage(products, title, false, true);
        
        std::cout << html;

    } catch (const std::exception& e) {
        std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";
        std::cout << "<html><body><h1>Помилка</h1><p>" << e.what() << "</p></body></html>";
    }
    return 0;
}