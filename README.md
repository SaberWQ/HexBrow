# 🌐 HexBrow

**Розумний браузер та пошукова система товарів**

[![macOS](https://img.shields.io/badge/macOS-10.15+-blue.svg)](https://www.apple.com/macos/)
[![C++](https://img.shields.io/badge/C++-17-red.svg)](https://isocpp.org/)
[![Objective-C++](https://img.shields.io/badge/Objective--C++-ARC-orange.svg)](https://en.wikipedia.org/wiki/Objective-C)
[![SQLite](https://img.shields.io/badge/SQLite-3.0+-003B57.svg)](https://www.sqlite.org/)

<div align="center">
  <img src="docs/images/main-preview.png" alt="HexBrow Main Window" width="800">
</div>

---

HexBrow — це веб-орієнтована пошукова система товарів, створена як курсова робота. Проєкт складається з двох основних частин:

1. **CGI-програма на C++**: отримує пошуковий запит від веб-сервера Apache, шукає товари в локальній базі SQLite і повертає динамічну HTML-сторінку з результатами.
2. **GUI-застосунок для macOS**: нативний додаток з каталогом товарів, вкладками та пошуком у мережі.

---

## 🚀 Як користуватися

### Запустити веб-версію (сервер + браузер)
```bash
make web
```
Ця команда:
1. Збере сервер.
2. Зупинить старий сервер (якщо він працює).
3. Запустить новий сервер.
4. Автоматично відкриє браузер (Chrome/Safari) із веб-інтерфейсом.

### Запустити нативний додаток для macOS
```bash
make mac
```
Ця команда:
1. Збере GUI-застосунок (`HexBrow.app`).
2. Відкриє його як звичайну програму.

### Перезапустити сервер
Якщо сервер завис або потрібно оновити його стан:
```bash
make restart
```

### Зупинити сервер
Щоб зупинити сервер:
```bash
make stop
```

### Перевірити стан
Щоб перевірити, чи працює сервер:
```bash
make info
```

---

## 🛠 Технології

- **C++17** (з `-Wall -Wextra`)
- **SQLite 3** (чистий C API)
- **Apache HTTP Server** з модулем CGI
- **Objective-C++** (ARC), Cocoa/AppKit, WebKit (WKWebView)
- **libcurl**, **nlohmann/json**

---

## 📋 Вимоги

### Для CGI
- Компілятор C++ з підтримкою C++17 (clang++ або g++).
- Бібліотека SQLite3.
- Веб-сервер Apache з увімкненим `mod_cgi`.

### Для GUI
- macOS 10.15 або новіша.
- Xcode Command Line Tools.
- Бібліотеки SQLite3 і libcurl (входять до macOS SDK).

---

## 🧩 Структура проєкту

```text
HexBrow/
├── src/
│   ├── Product.h               # Структура даних товару
│   ├── DatabaseManager.h/.cpp  # Клас для роботи з SQLite (CRUD, пошук, міграція)
│   ├── HtmlGenerator.h/.cpp    # Генерація HTML для CGI та GUI, екранування
│   ├── NetworkManager.h/.cpp   # HTTP-запити до API (для GUI)
│   ├── cgi_main.cpp            # Точка входу CGI-програми
│   ├── json.hpp                # nlohmann/json (сторонній заголовок)
│   └── resurces/
│       └── index.html          # Сторінка з формою пошуку
├── browser.mm                  # Точка входу GUI-застосунку (Cocoa + WebKit)
├── Makefile                    # Збірка CGI (make cgi) і GUI (make HexBrow)
└── README.md
```

---

## 🛡 Безпека

- Усі SQL-запити параметризовані, SQL-ін'єкції виключені.
- HTML-вихід екранується функцією `htmlEscape` (для тексту та атрибутів).
- Символи `%` і `_` в пошуковому запиті екрануються, тому не діють як wildcard.
- У `href` виводяться лише посилання з `http://` або `https://`.

---

