#pragma once
#include <string>

struct Product {
    int id;
    std::string name;
    std::string code;
    std::string manufacturer;
    std::string description;
    std::string imageUrl;
    std::string price;
    std::string link;
    int isFavorite; // 1 - так, 0 - ні
};