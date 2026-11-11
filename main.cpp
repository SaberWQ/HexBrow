#include <iostream>
#include <cstdlib>
#include <thread>
#include <chrono>

int main() {
#ifdef _WIN32
    std::system("taskkill /F /IM web_server.exe /T 2>nul");
#else
    std::system("killall web_server 2>/dev/null");
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Запускаємо HexBrow сервер...\n";
#ifdef _WIN32
    std::system("start /B web_server.exe --serve 8080");
#else
    std::system("./web_server --serve 8080 &");
#endif

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "Відкриваємо браузер...\n";
#ifdef _WIN32
    std::system("start http://127.0.0.1:8080");
#elif __APPLE__
    std::system("open http://127.0.0.1:8080");
#elif __linux__
    std::system("xdg-open http://127.0.0.1:8080");
#endif
    return 0;
}