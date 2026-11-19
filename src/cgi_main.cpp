/*
 * ============================================================================
 * HexBrow — Main HTTP Server + CGI Entry Point
 * ============================================================================
 *
 * УКР: Цей файл містить точку входу для двох режимів роботи:
 *      1) CGI-режим — для інтеграції з Apache (вимога методички).
 *      2) Standalone HTTP-сервер — для кросплатформного запуску.
 *
 *      Сервер реалізовано на POSIX-сокетах (Windows — Winsock).
 *      Усі запити обробляються послідовно в одному потоці.
 *
 * ENG: This file contains the entry point for two modes:
 *      1) CGI mode — for Apache integration (coursework requirement).
 *      2) Standalone HTTP server — for cross-platform deployment.
 *
 *      The server is implemented on POSIX sockets (Windows — Winsock).
 *      All requests are handled sequentially in a single thread.
 * ============================================================================
 */

 #include <iostream>
 #include <string>
 #include <cstdlib>
 #include <sstream>
 #include <cstring>
 #include <map>
 #include <vector>
 #include <fstream>
 #include <sys/stat.h>
 #include <ctime>
 #include <random>
 #include <csignal>
 #include <filesystem>
 
 /* УКР: Внутрішні модулі проєкту / ENG: Internal project modules */
 #include "DatabaseManager.h"
 #include "HtmlGenerator.h"
 #include "NetworkManager.h"
 #include "Favicon.h"
 
 /* ============================================================================
  * CROSS-PLATFORM SOCKET HEADERS
  * УКР: Різні API для Windows (Winsock) та Unix (POSIX).
  * ENG: Different APIs for Windows (Winsock) and Unix (POSIX).
  * ============================================================================ */
 #ifdef _WIN32
     #include <winsock2.h>
     #include <ws2tcpip.h>
     #pragma comment(lib, "ws2_32.lib")                        // Auto-link Winsock library
     #include <direct.h>                                       // For _mkdir()
     typedef SOCKET socket_t;                                  // Windows socket type
 #else
     #include <sys/socket.h>
     #include <netinet/in.h>
     #include <unistd.h>
     #include <arpa/inet.h>
     typedef int socket_t;                                     // Unix socket type
 #endif
 
 namespace fs = std::filesystem;
 
 /* ============================================================================
  * GLOBAL STATE
  * УКР: Глобальний стан сервера. Сесії — у пам'яті (для демо).
  * ENG: Global server state. Sessions — in memory (for demo).
  * ============================================================================ */
 std::map<std::string, std::string> activeSessions;            // sessionId -> email
 std::string globalExeDir = "";                                // Directory of the binary
 
 /* ============================================================================
  * FUNCTION: generateSessionId
  * ============================================================================
  * УКР: Генерує криптографічно випадковий 32-символьний sessionId.
  *      Використовує std::random_device як джерело ентропії.
  *      Це захищає від перебору (на відміну від "sess_0", "sess_1").
  *
  * ENG: Generates a cryptographically random 32-char session ID.
  *      Uses std::random_device as the entropy source.
  *      This prevents brute-force (unlike "sess_0", "sess_1").
  * ============================================================================ */
 std::string generateSessionId() {
     std::random_device rd;                                    // True entropy source
     std::mt19937 gen(rd());                                   // Mersenne Twister
     std::uniform_int_distribution<> dis(0, 15);               // 0..15 range
     const char* hex = "0123456789abcdef";                     // Hex alphabet
     std::string id;
     for (int i = 0; i < 32; ++i) id += hex[dis(gen)];         // 32 hex chars
     return id;
 }
 
 /* ============================================================================
  * BASE64 DECODER
  * УКР: Статичний алфавіт та хелпери для декодування Base64.
  * ENG: Static alphabet and helpers for Base64 decoding.
  * ============================================================================ */
 static const std::string base64_chars = 
     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 
 /*
  * УКР: Перевіряє, чи символ належить до Base64-алфавіту.
  * ENG: Checks whether the char belongs to the Base64 alphabet.
  */
 static inline bool is_base64(unsigned char c) {
     return (isalnum(c) || (c == '+') || (c == '/'));          // Valid Base64 char
 }
 
 /* ============================================================================
  * FUNCTION: base64_decode
  * ============================================================================
  * УКР: Декодує Base64-рядок у бінарні дані.
  *      Використовується для збереження зображень, що прийшли з форми.
  *      Вхід: рядок після "data:image/...;base64,".
  *      Вихід: сирі байти (std::string може містити \0).
  *
  * ENG: Decodes a Base64 string into binary data.
  *      Used to save images received from the form.
  *      Input: string after "data:image/...;base64,".
  *      Output: raw bytes (std::string can contain \0).
  * ============================================================================ */
 std::string base64_decode(std::string const& encoded_string) {
     int in_len = encoded_string.size();
     int i = 0, j = 0, in_ = 0;
     unsigned char char_array_4[4], char_array_3[3];           // 4->3 byte groups
     std::string ret;
 
     while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
         char_array_4[i++] = encoded_string[in_]; in_++;       // Collect 4 chars
         if (i == 4) {
             for (i = 0; i < 4; i++) char_array_4[i] = base64_chars.find(char_array_4[i]); // Map to 6-bit
             char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
             char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
             char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
             for (i = 0; (i < 3); i++) ret += char_array_3[i]; // Append 3 bytes
             i = 0;
         }
     }
     if (i) {                                                  // Handle padding
         for (j = i; j < 4; j++) char_array_4[j] = 0;
         for (j = 0; j < 4; j++) char_array_4[j] = base64_chars.find(char_array_4[j]);
         char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
         char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
         char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
         for (j = 0; (j < i - 1); j++) ret += char_array_3[j]; // Append remaining
     }
     return ret;
 }
 
 /* ============================================================================
  * HELPER FUNCTIONS
  * ============================================================================ */
 
 /* ----------------------------------------------------------------------------
  * FUNCTION: getAbsolutePath
  * ----------------------------------------------------------------------------
  * УКР: Будує абсолютний шлях відносно каталогу виконуваного файлу.
  *      Це вирішує проблему залежності від CWD (поточної робочої директорії).
  *
  * ENG: Builds an absolute path relative to the binary directory.
  *      This solves the CWD (current working directory) dependency.
  * ---------------------------------------------------------------------------- */
 std::string getAbsolutePath(const std::string& relativePath) {
     if (globalExeDir.empty()) return relativePath;            // Fallback
     return (fs::path(globalExeDir) / relativePath).string();  // Join paths
 }
 
 /* ----------------------------------------------------------------------------
  * FUNCTION: urlDecode
  * ----------------------------------------------------------------------------
  * УКР: Декодує URL-кодовані символи.
  *      "%20" -> ' ', "+" -> ' ', "%XX" -> char(0xXX).
  *
  * ENG: Decodes URL-encoded characters.
  *      "%20" -> ' ', "+" -> ' ', "%XX" -> char(0xXX).
  * ---------------------------------------------------------------------------- */
 std::string urlDecode(const std::string& str) {
     std::string result;
     for (size_t i = 0; i < str.length(); ++i) {
         if (str[i] == '%' && i + 2 < str.length()) {          // %XX sequence
             int value = 0;
             std::istringstream(str.substr(i + 1, 2)) >> std::hex >> value;
             result += static_cast<char>(value);               // Convert hex to char
             i += 2;
         } else if (str[i] == '+') result += ' ';              // '+' -> space
         else result += str[i];                                // Literal char
     }
     return result;
 }
 
 /* ----------------------------------------------------------------------------
  * FUNCTION: getQueryParam
  * ----------------------------------------------------------------------------
  * УКР: Витягує параметр з URL-рядка.
  *      Приклад: "q=Dell" або "?q=Dell" або "&q=Dell".
  *      Шукає "?key=" або "&key=", щоб не сплутати "aq" з "q".
  *
  * ENG: Extracts a parameter from the URL query string.
  *      Example: "q=Dell" or "?q=Dell" or "&q=Dell".
  *      Looks for "?key=" or "&key=" to avoid confusing "aq" with "q".
  * ---------------------------------------------------------------------------- */
 std::string getQueryParam(const std::string& query, const std::string& key) {
     std::string searchKey = key + "=";
     size_t pos = query.find("?" + searchKey);                 // After '?'
     if (pos == std::string::npos) pos = query.find("&" + searchKey); // After '&'
     if (pos == std::string::npos && query.rfind(searchKey, 0) == 0) pos = 0; // At start
     if (pos != std::string::npos) {
         size_t start = pos + searchKey.length() + (pos == 0 ? 0 : 1);
         size_t end = query.find('&', start);                  // Next '&' or end
         return urlDecode(query.substr(start, end == std::string::npos ? std::string::npos : end - start));
     }
     return "";
 }
 
 /* ----------------------------------------------------------------------------
  * FUNCTION: getUserFromCookie
  * ----------------------------------------------------------------------------
  * УКР: Читає cookie "session" і повертає email користувача.
  *      Якщо сесії немає (або вона недійсна) — повертає порожній рядок.
  *
  * ENG: Reads the "session" cookie and returns the user's email.
  *      Returns empty string if the session is missing or invalid.
  * ---------------------------------------------------------------------------- */
 std::string getUserFromCookie(const std::string& request) {
     size_t cookiePos = request.find("Cookie: ");              // Cookie header
     if (cookiePos != std::string::npos) {
         size_t end = request.find("\r\n", cookiePos);         // End of header line
         std::string cookieStr = request.substr(cookiePos + 8, end - cookiePos - 8);
         size_t sessPos = cookieStr.find("session=");          // Find our cookie
         if (sessPos != std::string::npos) {
             std::string sessId = cookieStr.substr(sessPos + 8);
             if (activeSessions.count(sessId)) return activeSessions[sessId]; // Valid session
         }
     }
     return "";
 }
 
 /* ----------------------------------------------------------------------------
  * FUNCTION: saveBase64Image
  * ----------------------------------------------------------------------------
  * УКР: Зберігає Base64-зображення у папку static/ та повертає URL.
  *      Формати: JPG, PNG, GIF (визначаються за MIME-типом у заголовку).
  *      Якщо base64Data не є зображенням — повертає порожній рядок.
  *
  * ENG: Saves a Base64 image into static/ and returns its URL.
  *      Formats: JPG, PNG, GIF (detected via MIME type in header).
  *      Returns empty string if base64Data is not an image.
  * ---------------------------------------------------------------------------- */
 std::string saveBase64Image(const std::string& base64Data, const std::string& prefix) {
     if (base64Data.empty() || base64Data.find("data:image") != 0) return ""; // Not an image
     size_t commaPos = base64Data.find(",");                   // Find 'data:...,base64,'
     if (commaPos == std::string::npos) return "";
 
     /* Determine extension by MIME type */
     std::string header = base64Data.substr(0, commaPos);
     std::string ext = "jpg";
     if (header.find("png") != std::string::npos) ext = "png";
     else if (header.find("gif") != std::string::npos) ext = "gif";
 
     std::string pureBase64 = base64Data.substr(commaPos + 1);
     std::string decoded = base64_decode(pureBase64);          // Decode to bytes
     if (decoded.empty()) return "";                           // Decode failed
 
     /* Generate unique filename */
     std::string filename = prefix + "_" + std::to_string(time(NULL)) + "_" + std::to_string(rand() % 10000) + "." + ext;
     std::string filepath = getAbsolutePath("static/" + filename);
 
     std::ofstream outfile(filepath, std::ios::binary);        // Open binary
     if (!outfile) return "";                                  // Cannot create file
     outfile.write(decoded.c_str(), decoded.size());           // Write bytes
     outfile.close();
 
     return "/static/" + filename;                             // URL path
 }
 
 /* ----------------------------------------------------------------------------
  * FUNCTION: sendAll
  * ----------------------------------------------------------------------------
  * УКР: Надсилає всі дані через сокет.
  *      send() може надіслати менше, ніж просили, тому використовуємо цикл.
  *
  * ENG: Sends all data through the socket.
  *      send() may send fewer bytes than requested, so we use a loop.
  * ---------------------------------------------------------------------------- */
 void sendAll(socket_t sock, const std::string& data) {
     size_t total = 0;
     while (total < data.length()) {
         int sent = send(sock, data.c_str() + total, data.length() - total, 0);
         if (sent <= 0) break;                                 // Error or closed
         total += sent;                                        // Advance pointer
     }
 }
 
 /* ----------------------------------------------------------------------------
  * FUNCTION: getBaseHtml
  * ----------------------------------------------------------------------------
  * УКР: Базова HTML-обгортка для простих сторінок (вхід, реєстрація, помилки).
  *      Включає Favicon (data URI, без зовнішніх запитів).
  *
  * ENG: Base HTML wrapper for simple pages (login, register, errors).
  *      Includes Favicon (data URI, no external requests).
  * ---------------------------------------------------------------------------- */
 std::string getBaseHtml(const std::string& content, const std::string& title = "HexBrow") {
     return "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
            "<link rel='icon' href='" + FAVICON_DATA_URI + "'>"  // Inline favicon
            "<title>" + title + "</title>"
            "<style>"
            "body { font-family: -apple-system, sans-serif; background: #f0f2f5; display: flex; align-items: center; justify-content: center; min-height: 100vh; margin: 0; }"
            ".box { background: white; padding: 40px; border-radius: 15px; box-shadow: 0 4px 20px rgba(0,0,0,0.1); width: 400px; text-align: center; }"
            "h2 { color: #333; margin-bottom: 25px; font-size: 26px; }"
            "input, textarea { padding: 12px; width: 100%; margin: 8px 0; border: 1px solid #ddd; border-radius: 8px; box-sizing: border-box; font-family: inherit; font-size: 15px; outline: none; }"
            "input[type='file'] { border: none; padding: 0; }"
            "input:focus, textarea:focus { border-color: #007aff; box-shadow: 0 0 0 3px rgba(0,122,255,0.1); }"
            "button { padding: 12px; background: #007aff; color: white; border: none; border-radius: 8px; cursor: pointer; width: 100%; font-weight: bold; margin-top: 10px; font-size: 16px; transition: background 0.2s; }"
            "button:hover { background: #0056b3; }"
            ".reg-btn { background: #28a745; }"
            ".reg-btn:hover { background: #218838; }"
            ".link { display: block; margin-top: 20px; color: #007aff; text-decoration: none; font-size: 15px; }"
            ".link:hover { text-decoration: underline; }"
            ".error { color: #d32f2f; background: #ffe6e6; padding: 12px; border-radius: 8px; margin-bottom: 15px; font-size: 14px; }"
            ".success { color: #155724; background: #d4edda; padding: 12px; border-radius: 8px; margin-bottom: 15px; font-size: 14px; }"
            "</style></head><body><div class='box'>" + content + "</div></body></html>";
 }
 
 /* ============================================================================
  * CGI MODE
  * ============================================================================
  * УКР: Режим CGI для Apache.
  *      Читає QUERY_STRING зі змінних оточення, генерує HTML-відповідь
  *      і виводить її в stdout (Apache перенаправляє в браузер).
  *
  * ENG: CGI mode for Apache.
  *      Reads QUERY_STRING from environment variables, generates HTML,
  *      and writes it to stdout (Apache forwards it to the browser).
  * ============================================================================ */
 void runCgi() {
     const char* queryString = std::getenv("QUERY_STRING");    // Set by Apache
     std::string searchQuery = queryString ? getQueryParam(queryString, "q") : "";
 
     std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n"; // HTTP header
 
     try {
         DatabaseManager db(getAbsolutePath("products.db"));  // Absolute DB path
         db.initialize();
         auto products = db.searchProducts(searchQuery);
         std::string html = HtmlGenerator::generatePage(products, "Результати пошуку: " + searchQuery, "", 0, false);
         std::cout << html;                                    // Send to stdout
     } catch (const std::exception& e) {
         std::cout << "<h1>Error: " << e.what() << "</h1>";    // Error page
     }
 }
 
 /* ============================================================================
  * STANDALONE HTTP SERVER MODE
  * ============================================================================ */
 /*
  * УКР: Головна функція HTTP-сервера.
  *      Приймає з'єднання на 127.0.0.1:port, парсить HTTP-запит,
  *      обробляє маршрути (routing) та надсилає відповідь.
  *
  *      Ключові особливості:
  *      - Безпечне читання запиту: заголовки + тіло (Content-Length).
  *      - Захист від Path Traversal для /static/.
  *      - Ігнорування SIGPIPE (сервер не падає при закритті з'єднання).
  *      - Абсолютні шляхи до products.db та static/.
  *
  * ENG: Main HTTP server function.
  *      Accepts connections on 127.0.0.1:port, parses HTTP requests,
  *      routes them, and sends responses.
  *
  *      Key features:
  *      - Safe request reading: headers + body (Content-Length).
  *      - Path Traversal protection for /static/.
  *      - SIGPIPE ignored (server doesn't crash when client disconnects).
  *      - Absolute paths to products.db and static/.
  */
 void runServer(int port, const char* argv0) {
     /* Store executable directory for absolute paths */
     globalExeDir = fs::path(argv0).parent_path().string();
 
     /* Create static/ directory for uploaded images */
     std::string staticDir = getAbsolutePath("static");
 #ifdef _WIN32
     _mkdir(staticDir.c_str());                                // Windows
 #else
     mkdir(staticDir.c_str(), 0777);                           // Unix
 #endif
 
     /* Initialize Winsock on Windows, ignore SIGPIPE on Unix */
 #ifdef _WIN32
     WSADATA wsaData;
     if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;    // Winsock init
 #else
     signal(SIGPIPE, SIG_IGN);                                 // Prevent crash on write to closed socket
 #endif
 
     socket_t server_fd, new_socket;
     struct sockaddr_in address;
     int opt = 1;
     int addrlen = sizeof(address);
 
     /* Create TCP socket */
     if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) return; // socket() failed
 
     /* Allow port reuse (avoids "Address already in use") */
     if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) return;
 
     /* Bind to localhost only (security: not accessible from network) */
     address.sin_family = AF_INET;
     address.sin_addr.s_addr = inet_addr("127.0.0.1");         // Localhost only
     address.sin_port = htons(port);                           // Network byte order
     if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) return;
 
     /* Listen for connections (backlog = 10) */
     if (listen(server_fd, 10) < 0) return;
 
     std::cout << "Server started on http://127.0.0.1:" << port << "\n";
     std::cout << "Database: " << getAbsolutePath("products.db") << "\n";
     std::cout << "Static:   " << getAbsolutePath("static") << "\n";
 
     /* ============ MAIN ACCEPT LOOP ============ */
     while (true) {
         if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) continue;
 
         /* ============ READ HTTP REQUEST ============ */
         /*
          * УКР: Читаємо запит частинами, поки не отримаємо всі заголовки
          *      і все тіло (Content-Length).
          * ENG: Read request in chunks until we have all headers and body.
          */
         std::string request;
         char buffer[4096];
         size_t headerEnd = std::string::npos;
         size_t contentLength = 0;
         int bytesRead;
 
         while ((bytesRead = recv(new_socket, buffer, sizeof(buffer), 0)) > 0) {
             request.append(buffer, bytesRead);                // Append chunk
 
             /* Parse headers to find \r\n\r\n and Content-Length */
             if (headerEnd == std::string::npos) {
                 headerEnd = request.find("\r\n\r\n");         // End of headers
                 if (headerEnd != std::string::npos) {
                     size_t clPos = request.find("Content-Length: "); // Parse body size
                     if (clPos != std::string::npos && clPos < headerEnd) {
                         size_t clEnd = request.find("\r\n", clPos);
                         try { contentLength = std::stoul(request.substr(clPos + 16, clEnd - clPos - 16)); } catch (...) {}
                     }
                 }
             }
 
             /* Break when the entire body is received */
             if (headerEnd != std::string::npos) {
                 size_t currentBodySize = request.length() - (headerEnd + 4);
                 if (currentBodySize >= contentLength) break;  // Body complete
             }
         }
 
         /* Empty request — close and continue */
         if (request.empty()) {
 #ifdef _WIN32
             closesocket(new_socket);                          // Windows
 #else
             close(new_socket);                                // Unix
 #endif
             continue;
         }
 
         /* Parse request line: METHOD PATH HTTP/1.1 */
         std::string path = "/", method = "GET", body = "";
         size_t pStart = request.find(" "), pEnd = request.find(" HTTP/");
         if (pStart != std::string::npos && pEnd != std::string::npos) {
             method = request.substr(0, pStart);               // GET or POST
             path = request.substr(pStart + 1, pEnd - pStart - 1); // URL path
         }
         if (headerEnd != std::string::npos) body = request.substr(headerEnd + 4); // Body
 
         /* Split path from query string */
         std::string basePath = path;
         size_t qpos = basePath.find('?');
         if (qpos != std::string::npos) basePath = basePath.substr(0, qpos); // /login?x=1 -> /login
 
         /* ============ RESPONSE VARIABLES ============ */
         std::string html, extraHeaders = "", userEmail = getUserFromCookie(request);
         int statusCode = 200; std::string statusText = "OK";
         extraHeaders += "X-Content-Type-Options: nosniff\r\n"; // Security header
 
         /* ============ STATIC FILES (with Path Traversal protection) ============ */
         if (basePath.rfind("/static/", 0) == 0) {
             std::string filename = basePath.substr(8);        // Remove "/static/"
             fs::path requestedPath = fs::path(getAbsolutePath("static")) / filename;
             fs::path canonicalStatic = fs::canonical(getAbsolutePath("static"));
             fs::path canonicalReq = fs::weakly_canonical(requestedPath);
 
             /* Reject any path outside static/ */
             if (canonicalReq.string().rfind(canonicalStatic.string(), 0) != 0) {
                 sendAll(new_socket, "HTTP/1.1 403 Forbidden\r\n\r\n");
 #ifdef _WIN32
                 closesocket(new_socket);
 #else
                 close(new_socket);
 #endif
                 continue;
             }
 
             std::ifstream file(canonicalReq.string(), std::ios::binary);
             if (file) {
                 std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                 std::string contentType = "image/jpeg";
                 if (filename.find(".png") != std::string::npos) contentType = "image/png";
                 else if (filename.find(".gif") != std::string::npos) contentType = "image/gif";
                 else if (filename.find(".webp") != std::string::npos) contentType = "image/webp";
 
                 std::string response = "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: " + contentType + "\r\n"
                                        "Content-Length: " + std::to_string(fileContent.length()) + "\r\n\r\n" + fileContent;
                 sendAll(new_socket, response);
             } else {
                 sendAll(new_socket, "HTTP/1.1 404 Not Found\r\n\r\n");
             }
 #ifdef _WIN32
             closesocket(new_socket);
 #else
             close(new_socket);
 #endif
             continue;
         }
 
         /* Connect to database for all dynamic routes */
         DatabaseManager db(getAbsolutePath("products.db"));
         db.initialize();
 
         /* ============ ROUTE: / (home) ============ */
         if (basePath == "/" || basePath.empty()) {
             if (userEmail.empty()) {
                 statusCode = 302; statusText = "Found";
                 extraHeaders += "Location: /login\r\n";      // Redirect to login
                 html = "";
             } else {
                 User u = db.getUserProfile(userEmail);
                 auto products = db.getAllProducts();
                 html = HtmlGenerator::generatePage(products, "Каталог товарів", u.username, u.id, false);
             }
         }
         /* ============ ROUTE: /login ============ */
         else if (basePath == "/login") {
             if (method == "POST") {
                 std::string email = getQueryParam(body, "email");
                 std::string pass = getQueryParam(body, "password");
                 if (db.loginUser(email, pass)) {
                     std::string sessId = generateSessionId(); // Random session
                     activeSessions[sessId] = email;           // Store session
                     extraHeaders += "Set-Cookie: session=" + sessId + "; Path=/; HttpOnly; SameSite=Strict\r\n";
                     statusCode = 302; statusText = "Found"; extraHeaders += "Location: /\r\n"; html = "";
                 } else {
                     html = getBaseHtml("<h2>Помилка</h2><div class='error'>Невірна пошта або пароль</div><a href='/login' class='link'>Спробувати ще</a>", "Помилка входу");
                 }
             } else {
                 html = getBaseHtml(R"(
                     <h2>Вхід у HexBrow</h2>
                     <form action='/login' method='POST'>
                         <input type='email' name='email' placeholder='Email' required>
                         <input type='password' name='password' placeholder='Пароль' required>
                         <button type='submit'>Увійти</button>
                     </form>
                     <a href='/register' class='link'>Немає акаунту? Зареєструватися →</a>
                 )", "Вхід");
             }
         }
         /* ============ ROUTE: /register ============ */
         else if (basePath == "/register") {
             if (method == "POST") {
                 std::string nickname = getQueryParam(body, "nickname");
                 std::string email = getQueryParam(body, "email");
                 std::string pass = getQueryParam(body, "password");
                 std::string confirm = getQueryParam(body, "confirm");
 
                 if (pass != confirm) {                        // Password mismatch
                     html = getBaseHtml("<h2>Помилка</h2><div class='error'>Паролі не співпадають</div><a href='/register' class='link'>Назад</a>", "Помилка");
                 } else if (db.registerUser(nickname, email, pass)) { // Success
                     html = getBaseHtml("<h2>Успіх!</h2><div class='success'>Акаунт створено!</div><a href='/login' class='link'>Увійти →</a>", "Реєстрація");
                 } else {
                     html = getBaseHtml("<h2>Помилка</h2><div class='error'>Користувач з такою поштою вже існує</div><a href='/register' class='link'>Назад</a>", "Помилка");
                 }
             } else {
                 html = getBaseHtml(R"(
                     <h2>Реєстрація</h2>
                     <form action='/register' method='POST'>
                         <input type='text' name='nickname' placeholder='Ваш нікнейм' required>
                         <input type='email' name='email' placeholder='Email' required>
                         <input type='password' name='password' placeholder='Пароль (мін. 4 символи)' required>
                         <input type='password' name='confirm' placeholder='Повторіть пароль' required>
                         <button type='submit' class='reg-btn'>Зареєструватися</button>
                     </form>
                     <a href='/login' class='link'>Вже є акаунт? Увійти →</a>
                 )", "Реєстрація");
             }
         }
         /* ============ ROUTE: /logout ============ */
         else if (basePath == "/logout") {
             extraHeaders += "Set-Cookie: session=; Path=/; Max-Age=0; HttpOnly\r\n"; // Clear cookie
             statusCode = 302; statusText = "Found"; extraHeaders += "Location: /login\r\n"; html = "";
         }
         /* ============ AUTH GUARD ============ */
         else if (userEmail.empty()) {                         // Not authenticated
             statusCode = 302; statusText = "Found";
             extraHeaders += "Location: /login\r\n"; html = "";
         }
         /* ============ ROUTE: /search ============ */
         else if (basePath == "/search") {
             std::string q = getQueryParam(path, "q");
             User u = db.getUserProfile(userEmail);
             auto local = db.searchProducts(q);                // Local DB
             auto web = NetworkManager::fetchInternetProducts(q, 0, 10); // Internet API
             local.insert(local.end(), web.begin(), web.end()); // Merge results
             html = HtmlGenerator::generatePage(local, "Результати: " + q, u.username, u.id, false);
         }
         /* ============ ROUTE: /profile ============ */
         else if (basePath == "/profile") {
             if (method == "POST") {
                 std::string nick = getQueryParam(body, "nickname");
                 std::string av_b64 = getQueryParam(body, "avatar_b64");
                 std::string ds = getQueryParam(body, "desc");
 
                 std::string av = "";
                 if (!av_b64.empty()) av = saveBase64Image(av_b64, "avatar"); // Save avatar
                 if (av.empty()) {                             // Keep old avatar
                     User oldUser = db.getUserProfile(userEmail);
                     av = oldUser.avatar;
                 }
 
                 db.updateUserProfile(userEmail, nick, av, ds);
                 statusCode = 302; statusText = "Found"; extraHeaders += "Location: /profile\r\n"; html = "";
             } else {
                 User u = db.getUserProfile(userEmail);
                 std::string safeNick = htmlEscape(u.username); // Escape for XSS
                 std::string safeDesc = htmlEscape(u.description); // Escape for XSS
 
                 std::string avatarHtml = u.avatar.empty() ? 
                     "<div style='width:120px; height:120px; border-radius:50%; background:#e0e0e0; margin:0 auto 15px; display:flex; align-items:center; justify-content:center; font-size:40px;'>👤</div>" : 
                     "<img src='" + u.avatar + "' style='width:120px; height:120px; border-radius:50%; object-fit:cover; margin-bottom:15px;'>";
 
                 std::string content = "<h2>Мій профіль</h2>" + avatarHtml +
                     "<form action='/profile' method='POST'>"
                     "<input type='text' name='nickname' value='" + safeNick + "' placeholder='Нікнейм'><br>"
                     "<label style='text-align:left; display:block; font-size:12px; color:#666; margin-top:10px;'>Змінити аватарку:</label>"
                     "<input type='file' id='avatarFile' accept='image/*' onchange='encodeImage(this, \"avatar_b64\")'><br>"
                     "<input type='hidden' name='avatar_b64' id='avatar_b64'>"
                     "<textarea name='desc' rows='3' placeholder='Про себе'>" + safeDesc + "</textarea><br>"
                     "<button type='submit'>Зберегти зміни</button></form>"
                     "<script>"
                     "function encodeImage(input, hiddenFieldId) {"           // JS: File -> Base64
                     "  if (input.files && input.files[0]) {"
                     "    var reader = new FileReader();"
                     "    reader.onload = function(e) { document.getElementById(hiddenFieldId).value = e.target.result; };"
                     "    reader.readAsDataURL(input.files[0]);"
                     "  }"
                     "}"
                     "</script>"
                     "<a href='/' class='link'>← На головну</a>";
                 html = getBaseHtml(content, "Профіль");
             }
         }
         /* ============ ROUTE: /upload ============ */
         else if (basePath == "/upload") {
             if (method == "POST") {
                 Product p;
                 p.name = getQueryParam(body, "name");
                 p.price = getQueryParam(body, "price");
                 p.description = getQueryParam(body, "desc");
                 p.link = getQueryParam(body, "link");
 
                 std::string imgB64 = getQueryParam(body, "image_b64");
                 if (!imgB64.empty()) p.imageUrl = saveBase64Image(imgB64, "prod"); // Save image
 
                 p.code = "USR-" + std::to_string(rand() % 10000); // Auto code
                 User u = db.getUserProfile(userEmail);
                 p.manufacturer = u.username;                  // Owner nickname
                 p.owner_id = u.id;                            // Owner ID
 
                 if (!p.name.empty() && !p.price.empty()) {
                     db.addProduct(p);                         // Insert product
                     statusCode = 302; statusText = "Found"; extraHeaders += "Location: /\r\n"; html = "";
                 } else {
                     html = getBaseHtml("<h2>Помилка</h2><div class='error'>Заповніть назву та ціну!</div><a href='/upload' class='link'>Назад</a>", "Помилка");
                 }
             } else {
                 std::string content = R"(
                     <h2>Додати товар</h2>
                     <form action='/upload' method='POST'>
                         <input type='text' name='name' placeholder='Назва товару' required>
                         <input type='text' name='price' placeholder='Ціна (в грн)' required>
                         <label style='text-align:left; display:block; font-size:12px; color:#666; margin-top:10px;'>Фото товару:</label>
                         <input type='file' id='prodFile' accept='image/*' onchange='encodeImage(this, "image_b64")'>
                         <input type='hidden' name='image_b64' id='image_b64'>
                         <input type='text' name='link' placeholder='Посилання на магазин (https://...)'>
                         <textarea name='desc' rows='3' placeholder='Опис товару'></textarea>
                         <button type='submit' class='reg-btn'>Опублікувати</button>
                     </form>
                     <script>
                     function encodeImage(input, hiddenFieldId) {
                       if (input.files && input.files[0]) {
                         var reader = new FileReader();
                         reader.onload = function(e) { document.getElementById(hiddenFieldId).value = e.target.result; };
                         reader.readAsDataURL(input.files[0]);
                       }
                     }
                     </script>
                     <a href='/' class='link'>← На головну</a>
                 )";
                 html = getBaseHtml(content, "Додати товар");
             }
         }
         /* ============ ROUTE: /edit_product ============ */
         else if (basePath == "/edit_product") {
             int id = std::stoi(getQueryParam(path, "id"));
             Product p = db.getProductById(id);
 
             if (method == "POST") {
                 p.name = getQueryParam(body, "name");
                 p.price = getQueryParam(body, "price");
                 p.description = getQueryParam(body, "desc");
                 p.link = getQueryParam(body, "link");
 
                 std::string imgB64 = getQueryParam(body, "image_b64");
                 if (!imgB64.empty()) {                        // New image uploaded
                     std::string newUrl = saveBase64Image(imgB64, "prod");
                     if (!newUrl.empty()) p.imageUrl = newUrl;
                 }
 
                 db.updateProduct(p);                          // Update product
                 statusCode = 302; statusText = "Found"; extraHeaders += "Location: /product?id=" + std::to_string(id) + "\r\n"; html = "";
             } else {
                 std::string content = "<h2>Редагувати товар</h2>"
                     "<form action='/edit_product?id=" + std::to_string(id) + "' method='POST'>"
                     "<input type='text' name='name' value='" + htmlEscape(p.name) + "' required>"
                     "<input type='text' name='price' value='" + htmlEscape(p.price) + "' required>"
                     "<label style='text-align:left; display:block; font-size:12px; color:#666; margin-top:10px;'>Змінити фото:</label>"
                     "<input type='file' id='editFile' accept='image/*' onchange='encodeImage(this, \"image_b64\")'>"
                     "<input type='hidden' name='image_b64' id='image_b64'>"
                     "<input type='text' name='link' value='" + htmlEscape(p.link) + "' placeholder='Посилання на магазин'>"
                     "<textarea name='desc' rows='3'>" + htmlEscape(p.description) + "</textarea>"
                     "<button type='submit'>Зберегти зміни</button></form>"
                     "<script>"
                     "function encodeImage(input, hiddenFieldId) {"
                     "  if (input.files && input.files[0]) {"
                     "    var reader = new FileReader();"
                     "    reader.onload = function(e) { document.getElementById(hiddenFieldId).value = e.target.result; };"
                     "    reader.readAsDataURL(input.files[0]);"
                     "  }"
                     "}"
                     "</script>"
                     "<a href='/product?id=" + std::to_string(id) + "' class='link'>← Назад до товару</a>";
                 html = getBaseHtml(content, "Редагувати");
             }
         }
         /* ============ ROUTE: /delete_product ============ */
         else if (basePath == "/delete_product") {
             int id = std::stoi(getQueryParam(path, "id"));
             Product p = db.getProductById(id);
             User u = db.getUserProfile(userEmail);
             if (p.owner_id == u.id) db.deleteProduct(id);     // Only owner can delete
             statusCode = 302; statusText = "Found"; extraHeaders += "Location: /\r\n"; html = "";
         }
         /* ============ ROUTE: /product ============ */
         else if (basePath == "/product") {
             int id = std::stoi(getQueryParam(path, "id"));
             Product p = db.getProductById(id);
             User u = db.getUserProfile(userEmail);
 
             std::string img = p.imageUrl.empty() ? "" : "<img src='" + htmlEscape(p.imageUrl) + "' style='max-width:100%; border-radius:10px; margin-bottom:15px;'><br>";
             std::string link = p.link.empty() ? "" : "<p><a href='" + htmlEscape(p.link) + "' target='_blank' style='color:#007aff;'>Купити в мережі</a></p>";
             std::string ownerActions = "";
             if (p.owner_id == u.id && u.id != 0) {            // Owner-only actions
                 ownerActions = "<hr><a href='/edit_product?id=" + std::to_string(id) + "' class='link'>✏️ Редагувати</a>"
                                "<a href='/delete_product?id=" + std::to_string(id) + "' class='link' style='color:#dc3545;' onclick=\"return confirm('Видалити?');\">🗑️ Видалити</a>";
             }
 
             std::string content = "<h2>" + htmlEscape(p.name) + "</h2>" + img + 
                 "<p style='text-align:left; color:#666;'><b>Виробник:</b> " + htmlEscape(p.manufacturer) + "</p>"
                 "<p style='text-align:left;'><b>Опис:</b> " + htmlEscape(p.description) + "</p>"
                 "<p style='font-size:24px; color:#e63946; font-weight:bold;'>" + htmlEscape(p.price) + " ₴</p>" + link + ownerActions +
                 "<a href='/' class='link'>← На головну</a>";
             html = getBaseHtml(content, "Товар: " + p.name);
         }
         /* ============ 404 ============ */
         else {
             statusCode = 404; statusText = "Not Found";
             html = getBaseHtml("<h2>404</h2><p>Сторінку не знайдено</p><a href='/'>На головну</a>", "404");
         }
 
         /* ============ SEND RESPONSE ============ */
         std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText + "\r\n";
         response += "Content-Type: text/html; charset=utf-8\r\n" + extraHeaders + "Connection: close\r\n\r\n" + html;
         sendAll(new_socket, response);                        // Send full response
 
         /* Close client socket */
 #ifdef _WIN32
         closesocket(new_socket);
 #else
         close(new_socket);
 #endif
     }
 }
 
 /* ============================================================================
  * ENTRY POINT
  * ============================================================================
  * УКР: Визначає режим роботи:
  *      - якщо є змінна оточення REQUEST_METHOD — CGI-режим (Apache).
  *      - якщо аргумент --serve <port> — Standalone HTTP-сервер.
  *
  * ENG: Determines the operating mode:
  *      - if REQUEST_METHOD env var is set — CGI mode (Apache).
  *      - if --serve <port> argument is passed — Standalone HTTP server.
  * ============================================================================ */
 int main(int argc, char* argv[]) {
     /* CGI mode: Apache sets REQUEST_METHOD */
     if (std::getenv("REQUEST_METHOD") != nullptr) {
         runCgi();                                             // Run as CGI
         return 0;
     }
 
     /* Standalone mode: --serve 8080 */
     if (argc > 1 && std::string(argv[1]) == "--serve") {
         int port = (argc > 2) ? std::atoi(argv[2]) : 8080;   // Default port 8080
         runServer(port, argv[0]);
         return 0;
     }
 
     /* No arguments — print usage */
     std::cout << "Usage:\n";
     std::cout << "  " << argv[0] << " --serve <port>   # Standalone HTTP server\n";
     std::cout << "  REQUEST_METHOD=GET " << argv[0] << "  # CGI mode\n";
     return 0;
 }