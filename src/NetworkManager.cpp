#include "NetworkManager.h"
#include <curl/curl.h>
#include "json.hpp"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>

using json = nlohmann::json;

/*
 * УКР: Callback для libcurl — записує отримані дані у std::string.
 * ENG: libcurl callback — writes received data into std::string.
 */
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);  // Append to buffer
    return size * nmemb;
}

/*
 * УКР: Виконує HTTP GET-запит, повертає тіло відповіді.
 *      Логує помилки curl у stderr.
 * ENG: Performs HTTP GET, returns response body.
 *      Logs curl errors to stderr.
 */
static std::string fetchUrl(const std::string& url) {
    CURL* curl = curl_easy_init();                              // Init curl
    std::string readBuffer;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());       // Set URL
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); // Set callback
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer); // Set buffer
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);     // Follow redirects
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);           // 10s timeout
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "HexBrow/1.0"); // User-Agent

        CURLcode res = curl_easy_perform(curl);                 // Execute
        if (res != CURLE_OK) std::cerr << "Curl Error: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);                                // Cleanup
    }
    return readBuffer;
}

/*
 * УКР: URL-кодує рядок (кирилиця, пробіли, спецсимволи).
 * ENG: URL-encodes a string (cyrillic, spaces, special chars).
 */
static std::string urlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (!curl) return str;
    char* enc = curl_easy_escape(curl, str.c_str(), (int)str.length()); // Escape
    std::string result = enc ? enc : str;
    curl_free(enc);                                             // Free memory
    curl_easy_cleanup(curl);
    return result;
}

/*
 * УКР: Конвертує USD → UAH за курсом 41.5, форматує 2 знаки.
 * ENG: Converts USD → UAH at 41.5 rate, formats 2 decimals.
 */
static std::string formatUah(double usdPrice) {
    double uah = usdPrice * 41.5;                               // Conversion
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << uah;            // Format
    return ss.str();
}

/*
 * УКР: Генерує Google Shopping посилання.
 * ENG: Generates a Google Shopping link.
 */
static std::string googleSearchUrl(const std::string& query) {
    return "https://www.google.com/search?q=" + urlEncode(query + " купити");
}

/*
 * УКР: Агрегує товари з трьох API, перемішує, застосовує пагінацію.
 * ENG: Aggregates products from three APIs, shuffles, applies pagination.
 */
std::vector<Product> NetworkManager::fetchInternetProducts(const std::string& query, int skip, int limit) {
    std::vector<Product> allProducts;                           // Combined list

    /* ============ Source 1: dummyjson.com ============ */
    {
        std::string url = "https://dummyjson.com/products?limit=100";
        if (!query.empty()) url = "https://dummyjson.com/products/search?q=" + urlEncode(query) + "&limit=100";
        std::string data = fetchUrl(url);                       // Fetch JSON
        try {
            auto j = json::parse(data);                         // Parse JSON
            if (j.contains("products")) {
                for (auto& item : j["products"]) {
                    Product p;
                    p.id = 0;
                    p.name = item.value("title", "Unknown");
                    p.code = "DJ-" + std::to_string(item.value("id", 0));
                    p.manufacturer = item.value("brand", "Generic");
                    p.description = item.value("description", "");
                    p.imageUrl = item.value("thumbnail", "");
                    p.price = formatUah(item.value("price", 0.0)); // Convert USD→UAH
                    p.link = googleSearchUrl(p.manufacturer + " " + p.name);
                    p.isFavorite = 0;
                    allProducts.push_back(p);
                }
            }
        } catch (const std::exception& e) { std::cerr << "API Error: " << e.what() << std::endl; }
    }

    /* ============ Source 2: fakestoreapi.com (empty query only) ============ */
    if (query.empty()) {
        std::string data = fetchUrl("https://fakestoreapi.com/products");
        try {
            auto arr = json::parse(data);
            for (auto& item : arr) {
                Product p;
                p.id = 0;
                p.name = item.value("title", "Unknown");
                p.code = "FS-" + std::to_string(item.value("id", 0));
                p.manufacturer = "FakeStore";
                p.description = item.value("description", "");
                p.imageUrl = item.value("image", "");
                p.price = formatUah(item.value("price", 0.0));
                p.link = googleSearchUrl(p.name);
                p.isFavorite = 0;
                allProducts.push_back(p);
            }
        } catch (const std::exception& e) { std::cerr << "API Error: " << e.what() << std::endl; }
    }

    /* ============ Source 3: escuelajs.co (Platzi) ============ */
    {
        std::string url = "https://api.escuelajs.co/api/v1/products?offset=0&limit=100";
        if (!query.empty()) url = "https://api.escuelajs.co/api/v1/products/?title=" + urlEncode(query);
        std::string data = fetchUrl(url);
        try {
            auto arr = json::parse(data);
            for (auto& item : arr) {
                Product p;
                p.id = 0;
                p.name = item.value("title", "Unknown");
                p.code = "PL-" + std::to_string(item.value("id", 0));
                p.manufacturer = "Platzi Store";
                p.description = item.value("description", "");
                if (item.contains("images") && item["images"].is_array() && !item["images"].empty()) {
                    p.imageUrl = item["images"][0].get<std::string>(); // First image
                }
                p.price = formatUah(item.value("price", 0.0));
                p.link = googleSearchUrl(p.name);
                p.isFavorite = 0;
                allProducts.push_back(p);
            }
        } catch (const std::exception& e) { std::cerr << "API Error: " << e.what() << std::endl; }
    }

    std::random_device rd;                                      // Entropy source
    std::mt19937 g(rd());                                       // Generator
    std::shuffle(allProducts.begin(), allProducts.end(), g);    // Shuffle

    std::vector<Product> result;
    int start = std::min(skip, (int)allProducts.size());        // Slice start
    int end = std::min(skip + limit, (int)allProducts.size());  // Slice end
    for (int i = start; i < end; ++i) result.push_back(allProducts[i]);

    return result;
}