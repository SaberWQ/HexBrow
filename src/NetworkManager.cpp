#include "NetworkManager.h"
#include <curl/curl.h>
#include "json.hpp"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static std::string fetchUrl(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string readBuffer;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "HexBrow/1.0");
        
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "Curl Error: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

static std::string urlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (!curl) return str;
    char* enc = curl_easy_escape(curl, str.c_str(), (int)str.length());
    std::string result = enc ? enc : str;
    curl_free(enc);
    curl_easy_cleanup(curl);
    return result;
}

// Конвертація USD -> UAH. Повертає ЧИСТЕ число без символу ₴, 
// щоб HtmlGenerator міг однаково відформатувати і локальні, і інтернетні товари.
static std::string formatUah(double usdPrice) {
    double uah = usdPrice * 41.5; // Орієнтовний курс
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << uah;
    return ss.str();
}

static std::string googleSearchUrl(const std::string& query) {
    return "https://www.google.com/search?q=" + urlEncode(query + " купити");
}

std::vector<Product> NetworkManager::fetchInternetProducts(const std::string& query, int skip, int limit) {
    std::vector<Product> allProducts;

    // Джерело 1: dummyjson
    {
        std::string url = "https://dummyjson.com/products?limit=100";
        if (!query.empty()) url = "https://dummyjson.com/products/search?q=" + urlEncode(query) + "&limit=100";
        std::string data = fetchUrl(url);
        try {
            auto j = json::parse(data);
            if (j.contains("products")) {
                for (auto& item : j["products"]) {
                    Product p;
                    p.id = 0;
                    p.name = item.value("title", "Unknown");
                    p.code = "DJ-" + std::to_string(item.value("id", 0));
                    p.manufacturer = item.value("brand", "Generic");
                    p.description = item.value("description", "");
                    p.imageUrl = item.value("thumbnail", "");
                    p.price = formatUah(item.value("price", 0.0));
                    p.link = googleSearchUrl(p.manufacturer + " " + p.name);
                    p.isFavorite = 0;
                    allProducts.push_back(p);
                }
            }
        } catch (const std::exception& e) { std::cerr << "API Error: " << e.what() << std::endl; }
    }

    // Джерело 2: fakestoreapi
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

    // Джерело 3: escuelajs (Platzi)
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
                    p.imageUrl = item["images"][0].get<std::string>();
                }
                p.price = formatUah(item.value("price", 0.0));
                p.link = googleSearchUrl(p.name);
                p.isFavorite = 0;
                allProducts.push_back(p);
            }
        } catch (const std::exception& e) { std::cerr << "API Error: " << e.what() << std::endl; }
    }

    // Перемішуємо
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(allProducts.begin(), allProducts.end(), g);

    // Застосовуємо skip та limit
    std::vector<Product> result;
    int start = std::min(skip, (int)allProducts.size());
    int end = std::min(skip + limit, (int)allProducts.size());
    for (int i = start; i < end; ++i) {
        result.push_back(allProducts[i]);
    }

    return result;
}