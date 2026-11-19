#pragma once
#include <string>

/*
 * УКР: Структура даних для зберігання інформації про користувача.
 *      Використовується для авторизації та профілю.
 * ENG: Data structure for storing user information.
 *      Used for authentication and profile.
 */
struct User {
    int id;                    // Unique user ID
    std::string username;      // Display nickname
    std::string email;         // Email (login)
    std::string password;      // Password hash (never plaintext!)
    std::string avatar;        // URL or path to avatar
    std::string description;   // "About me" text
};