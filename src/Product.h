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
    int isFavorite;
    int owner_id;
};