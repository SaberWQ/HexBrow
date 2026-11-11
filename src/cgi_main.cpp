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
#include "DatabaseManager.h"
#include "HtmlGenerator.h"
#include "NetworkManager.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <direct.h>
typedef SOCKET socket_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
typedef int socket_t;
#endif

namespace fs = std::filesystem;

std::map<std::string, std::string> activeSessions;
std::string globalExeDir = "";

// --- ГЕНЕРАЦІЯ БЕЗПЕЧНИХ СЕСІЙ ---
std::string generateSessionId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    const char* hex = "0123456789abcdef";
    std::string id;
    for (int i = 0; i < 32; ++i) id += hex[dis(gen)];
    return id;
}

// --- БЕЗПЕЧНИЙ BASE64 ДЕКОДЕР ---
static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static inline bool is_base64(unsigned char c) { return (isalnum(c) || (c == '+') || (c == '/')); }
std::string base64_decode(std::string const& encoded_string) {
    int in_len = encoded_string.size();
    int i = 0, j = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;
    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) char_array_4[i] = base64_chars.find(char_array_4[i]);
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            for (i = 0; (i < 3); i++) ret += char_array_3[i];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 4; j++) char_array_4[j] = 0;
        for (j = 0; j < 4; j++) char_array_4[j] = base64_chars.find(char_array_4[j]);
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }
    return ret;
}

// --- ДОПОМІЖНІ ФУНКЦІЇ ---
std::string getAbsolutePath(const std::string& relativePath) {
    if (globalExeDir.empty()) return relativePath;
    return (fs::path(globalExeDir) / relativePath).string();
}

std::string urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value = 0; std::istringstream(str.substr(i + 1, 2)) >> std::hex >> value;
            result += static_cast<char>(value); i += 2;
        } else if (str[i] == '+') result += ' ';
        else result += str[i];
    }
    return result;
}

std::string getQueryParam(const std::string& query, const std::string& key) {
    std::string searchKey = key + "=";
    size_t pos = query.find("?" + searchKey);
    if (pos == std::string::npos) pos = query.find("&" + searchKey);
    if (pos == std::string::npos && query.rfind(searchKey, 0) == 0) pos = 0;
    if (pos != std::string::npos) {
        size_t start = pos + searchKey.length() + (pos == 0 ? 0 : 1);
        size_t end = query.find('&', start);
        return urlDecode(query.substr(start, end == std::string::npos ? std::string::npos : end - start));
    }
    return "";
}

std::string getUserFromCookie(const std::string& request) {
    size_t cookiePos = request.find("Cookie: ");
    if (cookiePos != std::string::npos) {
        size_t end = request.find("\r\n", cookiePos);
        std::string cookieStr = request.substr(cookiePos + 8, end - cookiePos - 8);
        size_t sessPos = cookieStr.find("session=");
        if (sessPos != std::string::npos) {
            std::string sessId = cookieStr.substr(sessPos + 8);
            if (activeSessions.count(sessId)) return activeSessions[sessId];
        }
    }
    return "";
}

std::string saveBase64Image(const std::string& base64Data, const std::string& prefix) {
    if (base64Data.empty() || base64Data.find("data:image") != 0) return "";
    size_t commaPos = base64Data.find(",");
    if (commaPos == std::string::npos) return "";
    
    std::string header = base64Data.substr(0, commaPos);
    std::string ext = "jpg";
    if (header.find("png") != std::string::npos) ext = "png";
    else if (header.find("gif") != std::string::npos) ext = "gif";
    
    std::string pureBase64 = base64Data.substr(commaPos + 1);
    std::string decoded = base64_decode(pureBase64);
    if (decoded.empty()) return "";
    
    std::string filename = prefix + "_" + std::to_string(time(NULL)) + "_" + std::to_string(rand() % 10000) + "." + ext;
    std::string filepath = getAbsolutePath("static/" + filename);
    
    std::ofstream outfile(filepath, std::ios::binary);
    if (!outfile) return "";
    outfile.write(decoded.c_str(), decoded.size());
    outfile.close();
    
    return "/static/" + filename;
}

void sendAll(socket_t sock, const std::string& data) {
    size_t total = 0;
    while (total < data.length()) {
        int sent = send(sock, data.c_str() + total, data.length() - total, 0);
        if (sent <= 0) break;
        total += sent;
    }
}

std::string getBaseHtml(const std::string& content, const std::string& title = "HexBrow") {
    return "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>" + title + "</title>"
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

void runServer(int port, const char* argv0) {
    globalExeDir = fs::path(argv0).parent_path().string();
    
    std::string staticDir = getAbsolutePath("static");
    #ifdef _WIN32
        _mkdir(staticDir.c_str());
    #else
        mkdir(staticDir.c_str(), 0777);
    #endif

#ifdef _WIN32
    WSADATA wsaData; if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;
#else
    signal(SIGPIPE, SIG_IGN); // Ігноруємо SIGPIPE
#endif

    socket_t server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) return;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) return;
    address.sin_family = AF_INET; address.sin_addr.s_addr = inet_addr("127.0.0.1"); address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) return;
    if (listen(server_fd, 10) < 0) return;

    std::cout << "Server started on http://127.0.0.1:" << port << "\n";
    
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) continue;
        
        std::string request;
        char buffer[4096];
        size_t headerEnd = std::string::npos;
        size_t contentLength = 0;
        int bytesRead;

        while ((bytesRead = recv(new_socket, buffer, sizeof(buffer), 0)) > 0) {
            request.append(buffer, bytesRead);
            if (headerEnd == std::string::npos) {
                headerEnd = request.find("\r\n\r\n");
                if (headerEnd != std::string::npos) {
                    size_t clPos = request.find("Content-Length: ");
                    if (clPos != std::string::npos && clPos < headerEnd) {
                        size_t clEnd = request.find("\r\n", clPos);
                        try { contentLength = std::stoul(request.substr(clPos + 16, clEnd - clPos - 16)); } catch (...) {}
                    }
                }
            }
            if (headerEnd != std::string::npos) {
                size_t currentBodySize = request.length() - (headerEnd + 4);
                if (currentBodySize >= contentLength) break;
            }
        }
        
        if (request.empty()) {
#ifdef _WIN32
            closesocket(new_socket);
#else
            close(new_socket);
#endif
            continue;
        }

        std::string path = "/", method = "GET", body = "";
        size_t pStart = request.find(" "), pEnd = request.find(" HTTP/");
        if (pStart != std::string::npos && pEnd != std::string::npos) {
            method = request.substr(0, pStart);
            path = request.substr(pStart + 1, pEnd - pStart - 1);
        }
        if (headerEnd != std::string::npos) body = request.substr(headerEnd + 4);

        std::string basePath = path;
        size_t qpos = basePath.find('?');
        if (qpos != std::string::npos) basePath = basePath.substr(0, qpos);
        
        std::string html, extraHeaders = "", userEmail = getUserFromCookie(request);
        int statusCode = 200; std::string statusText = "OK";
        extraHeaders += "X-Content-Type-Options: nosniff\r\n";

        // ============ ВІДДАЧА СТАТИЧНИХ ФАЙЛІВ (БЕЗПЕЧНО) ============
        if (basePath.rfind("/static/", 0) == 0) {
            std::string filename = basePath.substr(8);
            // ЗАХИСТ ВІД PATH TRAVERSAL
            fs::path requestedPath = fs::path(getAbsolutePath("static")) / filename;
            fs::path canonicalStatic = fs::canonical(getAbsolutePath("static"));
            fs::path canonicalReq = fs::weakly_canonical(requestedPath);

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
                
                std::string response = "HTTP/1.1 200 OK\r\nContent-Type: " + contentType + "\r\nContent-Length: " + std::to_string(fileContent.length()) + "\r\n\r\n" + fileContent;
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

        DatabaseManager db(getAbsolutePath("products.db")); db.initialize();

        // ============ ГОЛОВНА ============
        if (basePath == "/" || basePath.empty()) {
            if (userEmail.empty()) {
                statusCode = 302; statusText = "Found"; extraHeaders += "Location: /login\r\n"; html = "";
            } else {
                User u = db.getUserProfile(userEmail);
                auto products = db.getAllProducts();
                html = HtmlGenerator::generatePage(products, "Каталог товарів", u.username, u.id);
            }
        }
        // ============ ВХІД ============
        else if (basePath == "/login") {
            if (method == "POST") {
                std::string email = getQueryParam(body, "email");
                std::string pass = getQueryParam(body, "password");
                if (db.loginUser(email, pass)) {
                    std::string sessId = generateSessionId(); // БЕЗПЕЧНА СЕСІЯ
                    activeSessions[sessId] = email;
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
        // ============ РЕЄСТРАЦІЯ ============
        else if (basePath == "/register") {
            if (method == "POST") {
                std::string nickname = getQueryParam(body, "nickname");
                std::string email = getQueryParam(body, "email");
                std::string pass = getQueryParam(body, "password");
                std::string confirm = getQueryParam(body, "confirm");
                
                if (pass != confirm) {
                    html = getBaseHtml("<h2>Помилка</h2><div class='error'>Паролі не співпадають</div><a href='/register' class='link'>Назад</a>", "Помилка");
                } else if (db.registerUser(nickname, email, pass)) {
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
        // ============ ВИХІД ============
        else if (basePath == "/logout") {
            extraHeaders += "Set-Cookie: session=; Path=/; Max-Age=0; HttpOnly\r\n";
            statusCode = 302; statusText = "Found"; extraHeaders += "Location: /login\r\n"; html = "";
        }
        // ============ ПЕРЕВІРКА АВТОРИЗАЦІЇ ============
        else if (userEmail.empty()) {
            statusCode = 302; statusText = "Found"; extraHeaders += "Location: /login\r\n"; html = "";
        }
        // ============ ПОШУК ============
        else if (basePath == "/search") {
            std::string q = getQueryParam(path, "q");
            User u = db.getUserProfile(userEmail);
            auto local = db.searchProducts(q);
            auto web = NetworkManager::fetchInternetProducts(q, 0, 10);
            local.insert(local.end(), web.begin(), web.end());
            html = HtmlGenerator::generatePage(local, "Результати: " + q, u.username, u.id);
        }
        // ============ ПРОФІЛЬ ============
        else if (basePath == "/profile") {
            if (method == "POST") {
                std::string nick = getQueryParam(body, "nickname");
                std::string av_b64 = getQueryParam(body, "avatar_b64");
                std::string ds = getQueryParam(body, "desc");
                
                std::string av = "";
                if (!av_b64.empty()) av = saveBase64Image(av_b64, "avatar");
                if (av.empty()) {
                    User oldUser = db.getUserProfile(userEmail);
                    av = oldUser.avatar;
                }
                
                db.updateUserProfile(userEmail, nick, av, ds);
                statusCode = 302; statusText = "Found"; extraHeaders += "Location: /profile\r\n"; html = "";
            } else {
                User u = db.getUserProfile(userEmail);
                std::string avatarHtml = u.avatar.empty() ? 
                    "<div style='width:120px; height:120px; border-radius:50%; background:#e0e0e0; margin:0 auto 15px; display:flex; align-items:center; justify-content:center; font-size:40px;'>👤</div>" : 
                    "<img src='" + u.avatar + "' style='width:120px; height:120px; border-radius:50%; object-fit:cover; margin-bottom:15px;'>";
                
                // ЕКРАНУВАННЯ XSS
                std::string safeNick = htmlEscape(u.username);
                std::string safeDesc = htmlEscape(u.description);
                
                std::string content = "<h2>Мій профіль</h2>" + avatarHtml +
                    "<form action='/profile' method='POST'>"
                    "<input type='text' name='nickname' value='" + safeNick + "' placeholder='Нікнейм'><br>"
                    "<label style='text-align:left; display:block; font-size:12px; color:#666; margin-top:10px;'>Змінити аватарку:</label>"
                    "<input type='file' id='avatarFile' accept='image/*' onchange='encodeImage(this, \"avatar_b64\")'><br>"
                    "<input type='hidden' name='avatar_b64' id='avatar_b64'>"
                    "<textarea name='desc' rows='3' placeholder='Про себе'>" + safeDesc + "</textarea><br>"
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
                    "<a href='/' class='link'>← На головну</a>";
                html = getBaseHtml(content, "Профіль");
            }
        }
        // ============ ДОДАТИ ТОВАР ============
        else if (basePath == "/upload") {
            if (method == "POST") {
                Product p; 
                p.name = getQueryParam(body, "name"); 
                p.price = getQueryParam(body, "price");
                p.description = getQueryParam(body, "desc"); 
                p.link = getQueryParam(body, "link");
                
                std::string imgB64 = getQueryParam(body, "image_b64");
                if (!imgB64.empty()) p.imageUrl = saveBase64Image(imgB64, "prod");
                
                p.code = "USR-" + std::to_string(rand()%10000);
                User u = db.getUserProfile(userEmail);
                p.manufacturer = u.username; 
                p.owner_id = u.id;
                
                if (!p.name.empty() && !p.price.empty()) {
                    db.addProduct(p);
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
        // ============ РЕДАГУВАТИ ТОВАР ============
        else if (basePath == "/edit_product") {
            int id = std::stoi(getQueryParam(path, "id"));
            Product p = db.getProductById(id);
            
            if (method == "POST") {
                p.name = getQueryParam(body, "name");
                p.price = getQueryParam(body, "price");
                p.description = getQueryParam(body, "desc");
                p.link = getQueryParam(body, "link");
                
                std::string imgB64 = getQueryParam(body, "image_b64");
                if (!imgB64.empty()) {
                    std::string newUrl = saveBase64Image(imgB64, "prod");
                    if (!newUrl.empty()) p.imageUrl = newUrl;
                }
                
                db.updateProduct(p);
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
        // ============ ВИДАЛИТИ ТОВАР ============
        else if (basePath == "/delete_product") {
            int id = std::stoi(getQueryParam(path, "id"));
            Product p = db.getProductById(id);
            User u = db.getUserProfile(userEmail);
            if (p.owner_id == u.id) db.deleteProduct(id);
            statusCode = 302; statusText = "Found"; extraHeaders += "Location: /\r\n"; html = "";
        }
        // ============ СТОРІНКА ТОВАРУ ============
        else if (basePath == "/product") {
            int id = std::stoi(getQueryParam(path, "id"));
            Product p = db.getProductById(id);
            User u = db.getUserProfile(userEmail);
            
            std::string img = p.imageUrl.empty() ? "" : "<img src='" + htmlEscape(p.imageUrl) + "' style='max-width:100%; border-radius:10px; margin-bottom:15px;'><br>";
            std::string link = p.link.empty() ? "" : "<p><a href='" + htmlEscape(p.link) + "' target='_blank' style='color:#007aff;'>Купити в мережі</a></p>";
            std::string ownerActions = "";
            if (p.owner_id == u.id && u.id != 0) {
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
        else { 
            statusCode = 404; statusText = "Not Found"; 
            html = getBaseHtml("<h2>404</h2><p>Сторінку не знайдено</p><a href='/'>На головну</a>", "404"); 
        }

        std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText + "\r\n";
        response += "Content-Type: text/html; charset=utf-8\r\n" + extraHeaders + "Connection: close\r\n\r\n" + html;
        sendAll(new_socket, response);
#ifdef _WIN32
        closesocket(new_socket);
#else
        close(new_socket);
#endif
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--serve") {
        int port = (argc > 2) ? std::atoi(argv[2]) : 8080;
        runServer(port, argv[0]);
        return 0;
    }
    return 0;
}