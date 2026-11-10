#pragma once
#include "Product.h"
#include <vector>
#include <string>

class HtmlGenerator {
public:
    // Генерує повну HTML-сторінку для CGI (з header'ом) або для GUI
    static std::string generatePage(const std::vector<Product>& products, const std::string& title, bool isInternet, bool isCgi);
};