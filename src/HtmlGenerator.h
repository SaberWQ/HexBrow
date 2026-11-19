#pragma once
#include "Product.h"
#include <vector>
#include <string>

std::string htmlEscape(const std::string& s);                  // HTML escape (XSS)
std::string escapeJs(const std::string& str);                  // JS string escape
std::string formatPriceForDisplay(const std::string& priceStr); // "1200" -> "1 200 ₴"

/*
 * УКР: Генератор повної HTML-сторінки з сіткою товарів.
 * ENG: Full HTML page generator with product grid.
 */
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