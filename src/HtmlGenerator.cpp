#include "HtmlGenerator.h"
#include <sstream>

// Екранування для HTML (захист від XSS)
std::string htmlEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

// Екранування для JavaScript
std::string escapeJs(const std::string& str) {
    std::string r;
    for (char c : str) {
        switch (c) {
            case '\\': r += "\\\\"; break;
            case '\'': r += "\\'"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            default: r += c;
        }
    }
    return r;
}

// Форматування ціни (додає пробіли та символ гривні)
std::string formatPriceForDisplay(const std::string& priceStr) {
    if (priceStr.empty()) return "0";
    std::string intPart = priceStr;
    std::string fracPart = "";
    size_t dotPos = priceStr.find('.');
    if (dotPos != std::string::npos) {
        intPart = priceStr.substr(0, dotPos);
        std::string frac = priceStr.substr(dotPos + 1);
        if (frac != "00" && frac != "0") fracPart = "." + frac;
    }
    int n = (int)intPart.length() - 3;
    while (n > 0) { intPart.insert(n, " "); n -= 3; }
    return intPart + fracPart + " ₴";
}

std::string HtmlGenerator::generatePage(
    const std::vector<Product>& products, 
    const std::string& title, 
    const std::string& username, 
    int currentUserId,
    bool showSaveButton
) {
    std::stringstream html;
    
    html << R"(
        <!DOCTYPE html><html><head><meta charset='UTF-8'>
        <title>HexBrow - )" << htmlEscape(title) << R"(</title>
        <style>
            body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; padding: 0; background: #f0f2f5; margin: 0; }
            .topbar { background: #fff; padding: 15px 30px; display: flex; justify-content: space-between; align-items: center; box-shadow: 0 2px 10px rgba(0,0,0,0.05); position: sticky; top: 0; z-index: 100; }
            .topbar h1 { margin: 0; font-size: 24px; color: #1a1a1a; }
            .topbar .user-info { display: flex; align-items: center; gap: 15px; font-size: 15px; }
            .topbar a { text-decoration: none; color: #007aff; font-weight: 600; transition: color 0.2s; }
            .topbar a:hover { color: #0056b3; }
            .search-container { padding: 25px 30px; text-align: center; background: #f0f2f5; }
            .search-box { display: inline-flex; width: 100%; max-width: 700px; background: white; border-radius: 30px; box-shadow: 0 4px 15px rgba(0,0,0,0.08); overflow: hidden; border: 1px solid #e0e0e0; }
            .search-box input { flex-grow: 1; border: none; padding: 16px 25px; font-size: 16px; outline: none; }
            .search-box button { background: #007aff; color: white; border: none; padding: 0 35px; font-weight: bold; font-size: 16px; cursor: pointer; transition: background 0.2s; }
            .search-box button:hover { background: #0056b3; }
            .content { padding: 20px; }
            h1.page-title { text-align: center; color: #333; margin-bottom: 30px; font-size: 28px; }
            .grid { display: flex; flex-wrap: wrap; gap: 25px; justify-content: center; max-width: 1200px; margin: 0 auto; }
            .card { background: white; border-radius: 16px; width: 280px; overflow: hidden; box-shadow: 0 4px 12px rgba(0,0,0,0.05); display: flex; flex-direction: column; position: relative; transition: transform 0.2s, box-shadow 0.2s; }
            .card:hover { transform: translateY(-5px); box-shadow: 0 12px 24px rgba(0,0,0,0.1); }
            .card img { width: 100%; height: 200px; object-fit: cover; background: #f9f9f9; border-bottom: 1px solid #eee; }
            .card-content { padding: 15px; flex-grow: 1; display: flex; flex-direction: column; }
            .card h2 { font-size: 16px; color: #1a1a1a; margin: 0 0 10px 0; line-height: 1.4; }
            .card .price { color: #e63946; font-weight: bold; font-size: 20px; margin-bottom: 15px; }
            .card .brand { color: #666; font-size: 12px; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 5px; font-weight: bold; }
            .btn-group { margin-top: auto; display: flex; gap: 8px; flex-direction: column; }
            .btn { display: block; text-align: center; background: #007aff; color: white; text-decoration: none; padding: 12px; border-radius: 8px; font-weight: bold; border: none; cursor: pointer; font-size: 14px; transition: background 0.2s; }
            .btn:hover { background: #0056b3; }
            .btn-save { background: #28a745; }
            .btn-save:hover { background: #218838; }
            .btn-details { background: #6c757d; }
            .btn-details:hover { background: #5a6268; }
            .btn-edit { background: #ffc107; color: #000; }
            .btn-edit:hover { background: #e0a800; }
            .btn-delete { background: #dc3545; }
            .btn-delete:hover { background: #c82333; }
            .empty-state { text-align: center; width: 100%; color: #888; padding: 50px; font-size: 18px; }
        </style>
        </head><body>
        
        <div class='topbar'>
            <h1>HexBrow</h1>
            <div class='user-info'>
    )";

    if (!username.empty()) {
        html << "<span>Привіт, <b>" << htmlEscape(username) << "</b>!</span>";
        html << "<a href='/profile'>👤 Профіль</a>";
        html << "<a href='/upload'>📤 Додати товар</a>";
        html << "<a href='/logout' style='color:#dc3545;'>🚪 Вийти</a>";
    } else {
        html << "<a href='/login'>Увійти</a>";
        html << "<a href='/register'>Реєстрація</a>";
    }

    html << R"(
            </div>
        </div>
        
        <div class='search-container'>
            <form action='/search' method='GET' class='search-box'>
                <input type='text' name='q' placeholder='Шукати товари локально та в інтернеті...' autofocus>
                <button type='submit'>Знайти</button>
            </form>
        </div>

        <div class='content'>
    )";

    html << "<h1 class='page-title'>" << htmlEscape(title) << "</h1>";
    html << "<div class='grid'>";

    if (products.empty()) {
        html << "<div class='empty-state'><h2>Товарів не знайдено.</h2><p>Спробуйте змінити запит або додайте свій перший товар!</p></div>";
    } else {
        for (const auto& p : products) {
            html << "<div class='card'>";
            
            // Картинка з перевіркою на порожнечу
            std::string imgSrc = p.imageUrl.empty() ? "https://via.placeholder.com/280x200?text=No+Image" : p.imageUrl;
            html << "<img src='" << htmlEscape(imgSrc) << "' alt='product' onerror=\"this.src='https://via.placeholder.com/280x200?text=No+Image'\">";
            
            html << "<div class='card-content'>";
            html << "<div class='brand'>" << htmlEscape(p.manufacturer) << "</div>";
            html << "<h2>" << htmlEscape(p.name) << "</h2>";
            html << "<div class='price'>" << htmlEscape(formatPriceForDisplay(p.price)) << "</div>";
            
            html << "<div class='btn-group'>";
            
            // Кнопка "Детальніше"
            html << "<a href='/product?id=" << p.id << "' class='btn btn-details'>Детальніше</a>";
            
            // Кнопка "Купити в мережі" (якщо є посилання)
            if (!p.link.empty() && (p.link.find("http") == 0)) {
                html << "<a href='" << htmlEscape(p.link) << "' target='_blank' class='btn'>Купити в мережі</a>";
            }

            // Кнопки редагування та видалення (ТІЛЬКИ для власника товару)
            if (p.owner_id == currentUserId && currentUserId != 0) {
                html << "<a href='/edit_product?id=" << p.id << "' class='btn btn-edit'>✏️ Редагувати</a>";
                html << "<a href='/delete_product?id=" << p.id << "' class='btn btn-delete' onclick=\"return confirm('Ви впевнені, що хочете видалити цей товар?');\">🗑️ Видалити</a>";
            }

            // Кнопка збереження для GUI (якщо використовується старий режим)
            if (showSaveButton) {
                html << "<button class='btn btn-save' onclick=\"saveProduct('" 
                     << htmlEscape(escapeJs(p.name)) << "', '" << htmlEscape(escapeJs(p.code)) << "', '" 
                     << htmlEscape(escapeJs(p.manufacturer)) << "', '" << htmlEscape(escapeJs(p.description)) << "', '" 
                     << htmlEscape(escapeJs(p.imageUrl)) << "', '" << htmlEscape(escapeJs(p.price)) << "', '" 
                     << htmlEscape(escapeJs(p.link)) << "')\">💾 Зберегти</button>";
            }
            html << "</div></div></div>";
        }
    }
    html << "</div></div>";
    
    // JS для GUI
    if (showSaveButton) {
        html << R"(
            <script>
                function saveProduct(name, code, manufacturer, description, imageUrl, price, link) {
                    window.webkit.messageHandlers.saveToCatalog.postMessage({ name, code, manufacturer, description, imageUrl, price, link });
                }
            </script>
        )";
    }
    
    html << "</body></html>";
    return html.str();
}