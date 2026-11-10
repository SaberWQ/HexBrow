#include "HtmlGenerator.h"
#include <sstream>

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

std::string escapeJs(const std::string& str) {
    std::string res = str;
    size_t pos = 0;
    while ((pos = res.find("'", pos)) != std::string::npos) {
        res.replace(pos, 1, "\\'");
        pos += 2;
    }
    return res;
}

std::string HtmlGenerator::generatePage(const std::vector<Product>& products, const std::string& title, bool isInternet, bool isCgi) {
    std::stringstream html;
    
    if (isCgi) {
        html << "Content-Type: text/html; charset=utf-8\r\n\r\n";
    }

    html << R"(
        <!DOCTYPE html>
        <html><head><meta charset='UTF-8'>
        <style>
            body { font-family: -apple-system, sans-serif; padding: 30px; background: #eef2f5; margin: 0; }
            h1 { color: #333; margin-bottom: 20px; }
            .grid { display: flex; flex-wrap: wrap; gap: 20px; justify-content: center; margin-bottom: 30px; }
            .card { background: white; border-radius: 12px; padding: 0; width: 280px; overflow: hidden; box-shadow: 0 4px 12px rgba(0,0,0,0.08); transition: transform 0.2s; display: flex; flex-direction: column; position: relative; }
            .card:hover { transform: translateY(-5px); box-shadow: 0 8px 24px rgba(0,0,0,0.15); }
            .card img { width: 100%; height: 180px; object-fit: cover; background: #f9f9f9; }
            .card-content { padding: 15px; flex-grow: 1; display: flex; flex-direction: column; }
            .card h2 { font-size: 16px; color: #1a1a1a; margin: 0 0 10px 0; }
            .card .price { color: #e63946; font-weight: bold; font-size: 18px; margin-bottom: 10px; }
            .card .brand { color: #666; font-size: 12px; text-transform: uppercase; margin-bottom: 5px; }
            .btn-group { margin-top: auto; }
            .btn { display: block; text-align: center; background: #007aff; color: white; text-decoration: none; padding: 10px; border-radius: 6px; font-weight: bold; margin-bottom: 5px; cursor: pointer; border: none; width: 100%; font-size: 14px; box-sizing: border-box;}
            .btn:hover { background: #0056b3; }
            .btn-save { background: #28a745; }
            .btn-save:hover { background: #218838; }
            .error { color: red; background: #ffe6e6; padding: 20px; border-radius: 8px; width: 100%; text-align: center;}
            .star { position: absolute; top: 10px; right: 10px; font-size: 24px; cursor: pointer; text-shadow: 0 0 5px rgba(0,0,0,0.5); }
            .load-more-container { text-align: center; width: 100%; padding: 20px 0; }
            .btn-load-more { background: #333; color: white; padding: 15px 40px; font-size: 16px; border-radius: 30px; cursor: pointer; border: none; font-weight: bold; }
            .btn-load-more:hover { background: #555; }
        </style>
        </head><body>
    )";
    
    html << "<h1>" << htmlEscape(title) << "</h1><div class='grid'>";

    if (products.empty()) {
        html << "<div class='error'><h2>Товарів не знайдено.</h2></div>";
    } else {
        for (const auto& p : products) {
            html << "<div class='card'>";
            
            if (!isCgi) {
                std::string starColor = p.isFavorite == 1 ? "#FFD700" : "#ccc";
                html << "<div class='star' style='color: " << starColor << ";' onclick=\"toggleFav(" << p.id << ", " << p.isFavorite << ")\">★</div>";
            }

            std::string imgSrc = p.imageUrl.empty() ? "https://via.placeholder.com/280x180?text=No+Image" : p.imageUrl;
            html << "<img src='" << htmlEscape(imgSrc) << "' alt='product' onerror=\"this.src='https://via.placeholder.com/280x180?text=No+Image'\">";
            
            html << "<div class='card-content'>";
            html << "<div class='brand'>" << htmlEscape(p.manufacturer) << "</div>";
            html << "<h2>" << htmlEscape(p.name) << "</h2>";
            
            if (!p.price.empty()) html << "<div class='price'>" << htmlEscape(p.price) << " ₴</div>";
            
            html << "<div class='btn-group'>";
            
            if (!p.link.empty() && (p.link.find("http://") == 0 || p.link.find("https://") == 0)) {
                html << "<a href='" << htmlEscape(p.link) << "' target='_blank' class='btn'>Купити / Переглянути</a>";
            } else if (!isCgi) {
                html << "<button class='btn' onclick=\"alert('Посилання відсутнє')\">Купити</button>";
            }

            if (isInternet && !isCgi) {
                html << "<button class='btn btn-save' onclick=\"saveProduct('" 
                     << escapeJs(htmlEscape(p.name)) << "', '" 
                     << escapeJs(htmlEscape(p.code)) << "', '" 
                     << escapeJs(htmlEscape(p.manufacturer)) << "', '" 
                     << escapeJs(htmlEscape(p.description)) << "', '" 
                     << escapeJs(htmlEscape(p.imageUrl)) << "', '" 
                     << escapeJs(htmlEscape(p.price)) << "', '" 
                     << escapeJs(htmlEscape(p.link)) << "')\">💾 Зберегти</button>";
            }

            html << "</div></div></div>";
        }
    }
    html << "</div>";
    
    // Кнопка Показати ще
    if (isInternet && !isCgi) {
        html << "<div class='load-more-container'><button class='btn-load-more' onclick=\"loadMore()\">Показати ще товари</button></div>";
    }
    
    if (!isCgi) {
        html << R"(
            <script>
                function saveProduct(name, code, manufacturer, description, imageUrl, price, link) {
                    window.webkit.messageHandlers.saveToCatalog.postMessage({
                        name: name, code: code, manufacturer: manufacturer, 
                        description: description, imageUrl: imageUrl, price: price, link: link
                    });
                }
                function toggleFav(id, status) {
                    window.webkit.messageHandlers.toggleFavorite.postMessage({ id: id, status: status });
                }
                function loadMore() {
                    window.webkit.messageHandlers.loadMore.postMessage('');
                }
            </script>
        )";
    }
    html << "</body></html>";
    return html.str();
}