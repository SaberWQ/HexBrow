CXX = clang++
CXXFLAGS = -std=c++17 -I src -Wall -Wextra
LDFLAGS = -lsqlite3 -lcurl

.PHONY: all web mac run restart stop clean info

all: web_server HexBrow.app

# 1. Збірка сервера
web_server: src/cgi_main.cpp src/DatabaseManager.cpp src/NetworkManager.cpp src/HtmlGenerator.cpp
	$(CXX) $(CXXFLAGS) src/cgi_main.cpp src/DatabaseManager.cpp src/NetworkManager.cpp src/HtmlGenerator.cpp $(LDFLAGS) -o web_server

# Автоматичне створення Info.plist, якщо його немає
Info.plist:
	@echo '<?xml version="1.0" encoding="UTF-8"?>' > Info.plist
	@echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' >> Info.plist
	@echo '<plist version="1.0">' >> Info.plist
	@echo '<dict>' >> Info.plist
	@echo '    <key>CFBundleExecutable</key>' >> Info.plist
	@echo '    <string>HexBrow</string>' >> Info.plist
	@echo '    <key>CFBundleIdentifier</key>' >> Info.plist
	@echo '    <string>com.saberwq.hexbrow</string>' >> Info.plist
	@echo '    <key>CFBundleName</key>' >> Info.plist
	@echo '    <string>HexBrow</string>' >> Info.plist
	@echo '    <key>CFBundlePackageType</key>' >> Info.plist
	@echo '    <string>APPL</string>' >> Info.plist
	@echo '    <key>NSPrincipalClass</key>' >> Info.plist
	@echo '    <string>NSApplication</string>' >> Info.plist
	@echo '    <key>NSHighResolutionCapable</key>' >> Info.plist
	@echo '    <true/>' >> Info.plist
	@echo '    <key>NSAppTransportSecurity</key>' >> Info.plist
	@echo '    <dict>' >> Info.plist
	@echo '        <key>NSAllowsLocalNetworking</key>' >> Info.plist
	@echo '        <true/>' >> Info.plist
	@echo '    </dict>' >> Info.plist
	@echo '</dict>' >> Info.plist
	@echo '</plist>' >> Info.plist

# 2. Збірка GUI (Mac)
HexBrow.app: browser.mm web_server Info.plist
	@echo "=== Збірка HexBrow.app ==="
	mkdir -p HexBrow.app/Contents/MacOS
	mkdir -p HexBrow.app/Contents/Resources
	cp Info.plist HexBrow.app/Contents/Info.plist
	$(CXX) $(CXXFLAGS) -fobjc-arc browser.mm -framework Cocoa -framework WebKit -o HexBrow.app/Contents/MacOS/HexBrow

# 3. Запуск у браузері (Веб-версія)
web: web_server
	@echo "=== Зупиняємо старий сервер ==="
	@killall web_server 2>/dev/null || true
	@echo "=== Запускаємо сервер у фоні ==="
	@./web_server --serve 8080 & sleep 1
	@echo "=== Відкриваємо браузер ==="
	@open http://127.0.0.1:8080

# 4. Запуск нативного додатку (Mac)
mac: HexBrow.app
	@echo "=== Зупиняємо старий сервер ==="
	@killall web_server 2>/dev/null || true
	@echo "=== Відкриваємо HexBrow.app ==="
	@open HexBrow.app

# 5. За замовчуванням просто запускаємо Mac додаток
run: mac

# 6. Перезапуск сервера (без відкриття браузера)
restart: web_server
	@echo "=== Перезапуск сервера ==="
	@killall web_server 2>/dev/null || true
	@./web_server --serve 8080 & sleep 1
	@echo "✅ Сервер перезапущено на http://127.0.0.1:8080"

# 7. Зупинка сервера
stop:
	@echo "=== Зупинка сервера ==="
	@killall web_server 2>/dev/null || echo "Сервер не запущено."
	@echo "✅ Сервер зупинено."

# 8. Очищення
clean:
	@echo "=== Очищення ==="
	rm -rf HexBrow.app
	rm -f web_server HexBrowApp

# 9. Інформація про стан
info:
	@echo "=== Статус HexBrow ==="
	@echo "Сервер запущено:"
	@pgrep -l web_server || echo "  ❌ Ні"
	@echo ""
	@echo "Доступні команди:"
	@echo "  make web      - запустити веб-версію (у браузері)"
	@echo "  make mac      - запустити нативний додаток macOS"
	@echo "  make restart  - перезапустити сервер"
	@echo "  make stop     - зупинити сервер"
	@echo "  make clean    - видалити збірку"
	@echo "  make info     - показати цю довідку"