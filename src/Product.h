#pragma once
#include <string>

struct Product {
    int id;
    std::string name;
    std::string code;
    std::string manufacturer;
    std::string description;
    
    // Нові поля для інтернет-товарів
    std::string imageUrl;
    std::string price;
    std::string link;
};