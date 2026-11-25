# ============================================================================
# HexBrow — Universal Makefile
# ============================================================================
# УКР: Один Makefile для macOS, Linux і Windows (MSYS2 MINGW64).
#      make init   — встановити залежності
#      make build  — зібрати сервер + GUI
#      make run    — запустити нативний GUI (Mac/Linux)
#      make web    — запустити у браузері (усі платформи)
#      make stop   — зупинити сервер
#      make clean  — очистити збірку
# ENG: One Makefile for macOS, Linux and Windows (MSYS2 MINGW64).
# ============================================================================

# ---------- OS DETECTION ----------
ifeq ($(OS),Windows_NT)
    PLATFORM := WINDOWS
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        PLATFORM := MACOS
    else
        PLATFORM := LINUX
    endif
endif

# ---------- SOURCES ----------
SERVER_SRC   := src/cgi_main.cpp src/DatabaseManager.cpp src/NetworkManager.cpp src/HtmlGenerator.cpp
COMMON_FLAGS := -std=c++17 -I src -Wall -Wextra

# ---------- PLATFORM CONFIG ----------
ifeq ($(PLATFORM),MACOS)
    CXX          := clang++
    LDFLAGS      := -lsqlite3 -lcurl
    SERVER_OUT   := web_server
    GUI_OUT      := HexBrowApp
    GUI_SRC      := browser.mm
    GUI_FLAGS    := -fobjc-arc -framework Cocoa -framework WebKit
    BUILD_PREREQ := build-server build-gui bundle
    WEB_PREREQ   := build-server HexBrowWeb
    WEB_RUN      := ./HexBrowWeb
    NATIVE_RUN   := open HexBrow.app
    STOP_CMD     := killall web_server 2>/dev/null || true
    CLEAN_CMD    := rm -f web_server HexBrowApp HexBrowWeb && rm -rf HexBrow.app
endif

ifeq ($(PLATFORM),LINUX)
    CXX          := g++
    LDFLAGS      := -lsqlite3 -lcurl
    SERVER_OUT   := web_server
    GUI_OUT      := HexBrowLinux
    GUI_SRC      := gui/browser_linux.cpp
    GUI_FLAGS    := $(shell pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.1)
    BUILD_PREREQ := build-server build-gui
    WEB_PREREQ   := build-server HexBrowWeb
    WEB_RUN      := ./HexBrowWeb
    NATIVE_RUN   := ./HexBrowLinux
    STOP_CMD     := killall web_server 2>/dev/null || true
    CLEAN_CMD    := rm -f web_server HexBrowLinux HexBrowWeb
endif

ifeq ($(PLATFORM),WINDOWS)
    CXX          := g++
    LDFLAGS      := -lsqlite3 -lcurl -lws2_32
    SERVER_OUT   := web_server.exe
    GUI_OUT      := 
    GUI_SRC      := 
    GUI_FLAGS    := 
    BUILD_PREREQ := build-server
    WEB_PREREQ   := build-server HexBrowWeb.exe
    WEB_RUN      := ./HexBrowWeb.exe
    NATIVE_RUN   := echo "Native GUI requires WebView2 SDK. Use 'make web'."
    STOP_CMD     := taskkill //F //IM web_server.exe 2>/dev/null || true
    CLEAN_CMD    := rm -f web_server.exe HexBrowWeb.exe
endif

.PHONY: all init build build-server build-gui bundle run web stop clean db-clean favicon info help

all: build

# ============ INIT ============
init:
ifeq ($(PLATFORM),MACOS)
	@echo "==> macOS: перевірка Xcode CLT..."
	@xcode-select -p >/dev/null 2>&1 || xcode-select --install
	@echo "✅ macOS готовий (sqlite3 та libcurl вбудовані)"
endif
ifeq ($(PLATFORM),LINUX)
	@echo "==> Linux: встановлення залежностей..."
	@if command -v apt >/dev/null 2>&1; then \
		sudo apt update && sudo apt install -y build-essential pkg-config \
			libgtk-3-dev libwebkit2gtk-4.1-dev libsqlite3-dev libcurl4-openssl-dev; \
	elif command -v dnf >/dev/null 2>&1; then \
		sudo dnf install -y gcc-c++ make pkg-config \
			gtk3-devel webkit2gtk4.1-devel sqlite-devel libcurl-devel; \
	elif command -v pacman >/dev/null 2>&1; then \
		sudo pacman -S --needed base-devel pkg-config gtk3 webkit2gtk-4.1 sqlite curl; \
	fi
	@echo "✅ Linux готовий"
endif
ifeq ($(PLATFORM),WINDOWS)
	@echo "==> Windows (MSYS2 MINGW64): перевірка..."
	@which g++ >/dev/null 2>&1 && echo "✅ g++: OK" || echo "❌ Встановіть: pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-sqlite3 mingw-w64-x86_64-curl"
	@echo "✅ Windows готовий (запускайте з MINGW64 shell)"
endif

# ============ BUILD ============
build: $(BUILD_PREREQ)

build-server: $(SERVER_SRC)
	@echo "==> Збірка сервера ($(PLATFORM))..."
	$(CXX) $(COMMON_FLAGS) $(SERVER_SRC) $(LDFLAGS) -o $(SERVER_OUT)
	@echo "✅ $(SERVER_OUT)"

build-gui: $(GUI_SRC) build-server
ifeq ($(PLATFORM),WINDOWS)
	@echo "⚠️ Windows native GUI потребує WebView2 SDK. Використовуйте 'make web'."
else
	@echo "==> Збірка GUI ($(PLATFORM))..."
	$(CXX) $(COMMON_FLAGS) $(GUI_SRC) $(GUI_FLAGS) -o $(GUI_OUT)
	@echo "✅ $(GUI_OUT)"
endif

# ============ BUNDLE (macOS .app) ============
bundle: build-gui
ifeq ($(PLATFORM),MACOS)
	@echo "==> Створення HexBrow.app..."
	mkdir -p HexBrow.app/Contents/MacOS HexBrow.app/Contents/Resources
	cp macos/Info.plist HexBrow.app/Contents/Info.plist
	cp HexBrowApp web_server HexBrow.app/Contents/MacOS/
	cp icons/HexBrow.icns HexBrow.app/Contents/Resources/ 2>/dev/null || true
	touch HexBrow.app
	codesign --force --deep --sign - HexBrow.app 2>/dev/null || true
	@echo "✅ HexBrow.app готовий"
endif

# ============ LAUNCHER ============
HexBrowWeb: main.cpp
	$(CXX) $(COMMON_FLAGS) main.cpp -o HexBrowWeb

HexBrowWeb.exe: main.cpp
	$(CXX) $(COMMON_FLAGS) main.cpp -o HexBrowWeb.exe

# ============ RUN (native GUI) ============
run: build
	@$(STOP_CMD)
	@$(NATIVE_RUN)

# ============ WEB (browser) ============
web: $(WEB_PREREQ)
	@echo "==> Запуск HexBrowWeb..."
	@$(WEB_RUN)

# ============ STOP ============
stop:
	@$(STOP_CMD)
	@echo "✅ Зупинено"

# ============ CLEAN ============
clean:
	@echo "==> Очищення..."
	@$(CLEAN_CMD)
	@echo "✅ Готово"

# ============ UTILITY ============
db-clean:
	@rm -f products.db
	@echo "✅ products.db видалено"

favicon:
	python3 tools/make_favicon_header.py

info:
	@echo "============================================"
	@echo " HexBrow — статус"
	@echo "============================================"
	@echo " Платформа:  $(PLATFORM)"
	@echo " Компілятор: $(CXX)"
	@echo " Сервер:     $(SERVER_OUT)"
	@echo " GUI:        $(GUI_OUT)"
	@echo "============================================"
	@echo " make init      — встановити залежності"
	@echo " make build     — зібрати сервер + GUI"
	@echo " make run       — запустити нативний GUI (Mac/Linux)"
	@echo " make web       — запустити у браузері"
	@echo " make stop      — зупинити сервер"
	@echo " make clean     — очистити збірку"
	@echo " make db-clean  — видалити products.db"
	@echo " make favicon   — перегенерувати Favicon.h"
	@echo " make info      — ця довідка"
	@echo "============================================"

help: info