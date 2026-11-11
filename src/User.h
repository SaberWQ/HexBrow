#pragma once
#include <string>

struct User {
    int id;
    std::string username; 
    std::string email;
    std::string password;
    std::string avatar;
    std::string description;
};