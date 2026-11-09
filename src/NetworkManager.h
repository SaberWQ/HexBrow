#pragma once
#include "Product.h"
#include <vector>

class NetworkManager {
public:
    // Функція для отримання товарів з інтернету
    static std::vector<Product> fetchInternetProducts(const std::string& query);
};