#pragma once
#include "Product.h"
#include <vector>
#include <string>

// Оголошуємо допоміжні функції, щоб вони були доступні в інших файлах
std::string htmlEscape(const std::string& s);
std::string escapeJs(const std::string& str);
std::string formatPriceForDisplay(const std::string& priceStr);

class HtmlGenerator {
public:
    static std::string generatePage(
        const std::vector<Product>& products, 
        const std::string& title, 
        const std::string& username, 
        int currentUserId,
        bool showSaveButton = false
    );
};