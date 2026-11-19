#pragma once
#include "Product.h"
#include <vector>
#include <string>

/*
 * УКР: Клас для роботи з зовнішніми API.
 *      Агрегує товари з трьох безкоштовних публічних сервісів.
 * ENG: Class for working with external APIs.
 *      Aggregates products from three free public services.
 */
class NetworkManager {
public:
    /*
     * УКР: Завантажує товари з API (dummyjson, fakestoreapi, escuelajs).
     *      Підтримує пошук та пагінацію.
     * ENG: Fetches products from APIs (dummyjson, fakestoreapi, escuelajs).
     *      Supports search and pagination.
     */
    static std::vector<Product> fetchInternetProducts(const std::string& query, int skip = 0, int limit = 20);
};