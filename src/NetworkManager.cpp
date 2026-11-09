#include "NetworkManager.h"
#include <curl/curl.h>
#include "json.hpp"
#include <iostream>

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::vector<Product> NetworkManager::fetchInternetProducts(const std::string& query) {
    std::vector<Product> results;
    CURL* curl = curl_easy_init();
    std::string readBuffer;

    if (curl) {
        std::string url = "https://dummyjson.com/products/search?q=" + query;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        if (curl_easy_perform(curl) == CURLE_OK) {
            try {
                auto j = json::parse(readBuffer);
                if (j.contains("products")) {
                    for (auto& item : j["products"]) {
                        Product p;
                        p.id = item.value("id", 0);
                        p.name = item.value("title", "Unknown");
                        p.code = "INT-" + std::to_string(p.id);
                        p.manufacturer = item.value("brand", "Generic");
                        p.description = item.value("description", "");
                        
                        // Нові поля
                        p.imageUrl = item.value("thumbnail", "");
                        p.price = std::to_string(item.value("price", 0.0));
                        p.link = "https://dummyjson.com/products/" + std::to_string(p.id);
                        
                        results.push_back(p);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "JSON Error: " << e.what() << std::endl;
            }
        }
        curl_easy_cleanup(curl);
    }
    return results;
}