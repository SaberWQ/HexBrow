#!/bin/bash
echo "=== Збірка HexBrow ==="
clang++ -std=c++17 -I src src/cgi_main.cpp src/DatabaseManager.cpp src/NetworkManager.cpp src/HtmlGenerator.cpp -lsqlite3 -lcurl -o web_server
clang++ -std=c++17 -I src main.cpp -o HexBrowWeb
echo "=== Готово! Запускаємо... ==="
./HexBrowWeb