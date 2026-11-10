#pragma once
#include "Product.h"
#include <vector>
#include <string>

class NetworkManager {
public:
    static std::vector<Product> fetchInternetProducts(const std::string& query, int skip = 0, int limit = 20);
};