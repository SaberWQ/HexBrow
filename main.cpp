#include <iostream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <filesystem>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <climits>
#include <unistd.h>
#elif _WIN32
#include <windows.h>
#endif

static void chdirToExecutableDir() {
#ifdef __APPLE__
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        char real[PATH_MAX];
        if (realpath(buf, real)) {
            std::error_code ec;
            std::filesystem::current_path(std::filesystem::path(real).parent_path(), ec);
            if (ec) std::cerr << "chdir failed: " << ec.message() << "\n";
        }
    }
#elif _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::error_code ec;
    std::filesystem::current_path(std::filesystem::path(buf).parent_path(), ec);
#endif
}

int main() {
    chdirToExecutableDir();                                     // CWD = folder with HexBrowWeb

#ifdef _WIN32
    std::system("taskkill /F /IM web_server.exe >nul 2>&1");    // Windows kill
#else
    std::system("killall web_server 2>/dev/null");              // Mac/Linux kill
#endif

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Starting HexBrow server...\n";

#ifdef _WIN32
    // На Windows запускаємо без ./, використовуємо start /B для фону
    std::system("start /B web_server.exe --serve 8080");        
#else
    // На Mac/Linux залишаємо як було
    std::system("./web_server --serve 8080 &");                 
#endif

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "Opening browser...\n";
#ifdef _WIN32
    std::system("start http://127.0.0.1:8080");
#elif __APPLE__
    std::system("open http://127.0.0.1:8080");
#elif __linux__
    std::system("xdg-open http://127.0.0.1:8080");
#endif
    return 0;
}