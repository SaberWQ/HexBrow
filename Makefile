CXX = clang++
CXXFLAGS = -std=c++17 -I src -Wall -Wextra
LDFLAGS = -lsqlite3 -lcurl

APP      = HexBrow.app
APP_BIN  = $(APP)/Contents/MacOS
APP_RES  = $(APP)/Contents/Resources

.PHONY: all bundle mac web restart stop db-clean favicon clean info

all: mac

# ============================================================================
# УКР: Збірка C++ сервера.
# ENG: Build C++ server.
# ============================================================================
web_server: src/cgi_main.cpp src/DatabaseManager.cpp src/NetworkManager.cpp src/HtmlGenerator.cpp
	$(CXX) $(CXXFLAGS) src/cgi_main.cpp src/DatabaseManager.cpp src/NetworkManager.cpp src/HtmlGenerator.cpp $(LDFLAGS) -o web_server

# ============================================================================
# УКР: Збірка НАТИВНОГО Mac GUI (Cocoa + WebKit).
# ENG: Build NATIVE Mac GUI (Cocoa + WebKit).
# ============================================================================
HexBrowApp: browser.mm
	$(CXX) $(CXXFLAGS) -fobjc-arc browser.mm -framework Cocoa -framework WebKit -o HexBrowApp

# ============================================================================
# УКР: Збірка КРОСПЛАТФОРМНОГО веб-лаунчера (Windows/Linux).
# ENG: Build CROSS-PLATFORM web launcher (Windows/Linux).
# ============================================================================
HexBrowWeb: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o HexBrowWeb

# ============================================================================
# УКР: Збірка повноцінного .app бандлу з іконкою.
# ENG: Build full .app bundle with icon.
# ============================================================================
bundle: web_server HexBrowApp
	mkdir -p $(APP_BIN) $(APP_RES)
	cp macos/Info.plist $(APP)/Contents/Info.plist
	cp HexBrowApp web_server $(APP_BIN)/
	cp icons/HexBrow.icns $(APP_RES)/HexBrow.icns
	touch $(APP)  # Force Finder to refresh icon
	codesign --force --deep --sign - $(APP) 2>/dev/null || true

# ============================================================================
# УКР: Запуск нативного Mac додатку.
# ENG: Run native Mac app.
# ============================================================================
mac: bundle
	@killall web_server 2>/dev/null || true
	@killall HexBrowApp 2>/dev/null || true
	@open $(APP)

# ============================================================================
# УКР: Запуск веб-версії (для Linux/Windows або тестування в браузері).
# ENG: Run web version (for Linux/Windows or browser testing).
# ============================================================================
web: web_server HexBrowWeb
	@killall web_server 2>/dev/null || true
	@./web_server --serve 8080 & sleep 1
	@./HexBrowWeb

# ============================================================================
# УКР: Генерація Favicon.
# ENG: Generate Favicon.
# ============================================================================
favicon:
	python3 tools/make_favicon_header.py

# ============================================================================
# УКР: Очищення БД.
# ENG: Clean DB.
# ============================================================================
db-clean:
	rm -f products.db
	@echo "✅ Database removed."

# ============================================================================
# УКР: Зупинка сервера.
# ENG: Stop server.
# ============================================================================
stop:
	@killall web_server 2>/dev/null || echo "Server not running."

# ============================================================================
# УКР: Очищення артефактів.
# ENG: Clean artifacts.
# ============================================================================
clean:
	rm -rf $(APP)
	rm -f web_server HexBrowApp HexBrowWeb

# ============================================================================
# УКР: Інформація.
# ENG: Info.
# ============================================================================
info:
	@echo "=== HexBrow Status ==="
	@pgrep -l web_server || echo "  ❌ Server: not running"
	@echo ""
	@echo "Commands:"
	@echo "  make mac       - build & run native Mac app"
	@echo "  make web       - build & run web version"
	@echo "  make favicon   - regenerate Favicon.h"
	@echo "  make db-clean  - delete products.db"
	@echo "  make clean     - remove artifacts"