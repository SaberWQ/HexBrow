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

std::vector<Product> NetworkManager::fetchInternetProducts(const std::string& query, int skip, int limit) {
    std::vector<Product> results;
    CURL* curl = curl_easy_init();
    std::string readBuffer;
    
    // Курс долара до гривні (приблизний)
    const double USD_TO_UAH = 41.5;

    if (curl) {
        std::string url = "https://dummyjson.com/products?limit=" + std::to_string(limit) + "&skip=" + std::to_string(skip);
        if (!query.empty()) {
            url = "https://dummyjson.com/products/search?q=" + query + "&limit=" + std::to_string(limit) + "&skip=" + std::to_string(skip);
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        if (curl_easy_perform(curl) == CURLE_OK) {
            try {
                auto j = json::parse(readBuffer);
                if (j.contains("products")) {
                    for (auto& item : j["products"]) {
                        Product p;
                        p.id = 0;
                        p.name = item.value("title", "Unknown");
                        p.code = "INT-" + std::to_string(item.value("id", 0));
                        p.manufacturer = item.value("brand", "Internet Brand");
                        p.description = item.value("description", "");
                        p.imageUrl = item.value("thumbnail", "");
                        
                        // КОНВЕРТАЦІЯ ВАЛЮТИ
                        double usdPrice = item.value("price", 0.0);
                        double uahPrice = usdPrice * USD_TO_UAH;
                        std::stringstream ss;
                        ss << std::fixed << std::setprecision(2) << uahPrice;
                        p.price = ss.str();

                        // ГЛОБАЛЬНИЙ ПОШУК
                        std::string searchQuery = p.manufacturer + " " + p.name;
                        std::replace(searchQuery.begin(), searchQuery.end(), ' ', '+');
                        p.link = "https://www.google.com/search?q=" + searchQuery + "+купити";
                        p.isFavorite = 0;
                        
                        results.push_back(p);
                    }
                    
                    // ПЕРЕМІШУВАННЯ
                    std::random_device rd;
                    std::mt19937 g(rd());
                    std::shuffle(results.begin(), results.end(), g);
                }
            } catch (const std::exception& e) { std::cerr << "JSON Error: " << e.what() << std::endl; }
        }
        curl_easy_cleanup(curl);
    }
    return results;
}