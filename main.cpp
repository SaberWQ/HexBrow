#include <iostream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <filesystem>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <climits>
#include <unistd.h>
#endif

/*
 * УКР: Змінює поточну директорію на ту, де лежить бінарник.
 *      Це потрібно, щоб лаунчер знайшов web_server при подвійному кліку на .app.
 * ENG: Changes current directory to the binary's directory.
 *      Needed so the launcher finds web_server on double-click of .app.
 */
static void chdirToExecutableDir() {
#ifdef __APPLE__
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {                // Get exe path
        char real[PATH_MAX];
        if (realpath(buf, real)) {                              // Resolve symlinks
            std::filesystem::current_path(std::filesystem::path(real).parent_path()); // chdir
        }
    }
#endif
}

/*
 * УКР: Кросплатформний лаунчер. Зупиняє старий сервер, запускає новий,
 *      відкриває браузер.
 * ENG: Cross-platform launcher. Stops old server, starts new one,
 *      opens browser.
 */
int main() {
    chdirToExecutableDir();                                     // chdir to exe dir

    /* Step 1: kill old server */
#ifdef _WIN32
    std::system("taskkill /F /IM web_server.exe /T 2>nul");
#else
    std::system("killall web_server 2>/dev/null");              // Ignore error
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait

    /* Step 2: start new server in background */
    std::cout << "Starting HexBrow server...\n";
#ifdef _WIN32
    std::system("start /B web_server.exe --serve 8080");
#else
    std::system("./web_server --serve 8080 &");                 // Background
#endif

    std::this_thread::sleep_for(std::chrono::seconds(1));       // Wait for startup

    /* Step 3: open browser */
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