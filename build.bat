@echo off
echo === Building HexBrow ===

where g++ >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] g++ compiler not found!
    echo Please run this file inside MSYS2 MINGW64 terminal.
    pacman -S --needed make mingw-w64-x86_64-gcc mingw-w64-x86_64-sqlite3 mingw-w64-x86_64-curl
    pause
    exit /b
)

echo [1/2] Compiling server...c
g++ -std=c++17 -I src src/cgi_main.cpp src/DatabaseManager.cpp src/NetworkManager.cpp src/HtmlGenerator.cpp -lsqlite3 -lcurl -lws2_32 -o web_server.exe
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile server!
    pause
    exit /b
)

echo [2/2] Compiling launcher...
g++ -std=c++17 -I src main.cpp -o HexBrowWeb.exe
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile launcher!
    pause
    exit /b
)

echo === Success! Starting... ===
make init
make build
make run
pause