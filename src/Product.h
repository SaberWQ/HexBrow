#pragma once
#include <string>

/*
 * УКР: Структура даних для зберігання інформації про товар.
 *      Містить усі поля, необхідні для картки товару та БД.
 * ENG: Data structure for storing product information.
 *      Contains all fields required for a product card and database.
 */
struct Product {
    int id;                    // Unique database identifier
    std::string name;          // Product name
    std::string code;          // SKU or product code
    std::string manufacturer;  // Manufacturer or brand
    std::string description;   // Detailed description
    std::string imageUrl;      // URL or path to image
    std::string price;         // Price as string
    std::string link;          // Link to marketplace
    int isFavorite;            // 1 - favorite, 0 - no
    int owner_id;              // Owner user ID
};