#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#include "src/DatabaseManager.h" 
#include "src/NetworkManager.h"
#include <string>

@interface AppDelegate : NSObject <NSApplicationDelegate, NSTextFieldDelegate, NSSearchFieldDelegate, WKNavigationDelegate>
@property (strong, nonatomic) NSWindow *window;
@property (strong, nonatomic) WKWebView *webView;
@property (strong, nonatomic) NSSearchField *searchField;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // 1. Створюємо вікно
    NSRect frame = NSMakeRect(0, 0, 1100, 800); // Трохи більший розмір за замовчуванням
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    [self.window setTitle:@"HexBrow - Твій Розумний Браузер"];
    [self.window center];

    NSView *contentView = self.window.contentView;

    // 2. Створюємо верхню панель (Toolbar) з Auto Layout
    NSView *topBar = [[NSView alloc] init];
    topBar.translatesAutoresizingMaskIntoConstraints = NO;
    [topBar setWantsLayer:YES];
    topBar.layer.backgroundColor = [NSColor windowBackgroundColor].CGColor;
    [contentView addSubview:topBar];

    // 3. Поле пошуку (як у Safari)
    self.searchField = [[NSSearchField alloc] init];
    self.searchField.translatesAutoresizingMaskIntoConstraints = NO;
    [self.searchField setPlaceholderString:@"Введіть запит або URL (напр., dell або https://google.com)"];
    self.searchField.delegate = self;
    [topBar addSubview:self.searchField];

    // 4. Кнопки
    NSButton *localSearchBtn = [NSButton buttonWithTitle:@"Локально" target:self action:@selector(searchLocal:)];
    localSearchBtn.translatesAutoresizingMaskIntoConstraints = NO;
    localSearchBtn.bezelStyle = NSBezelStyleRounded;
    [topBar addSubview:localSearchBtn];

    NSButton *webSearchBtn = [NSButton buttonWithTitle:@"В Інтернеті" target:self action:@selector(searchInternet:)];
    webSearchBtn.translatesAutoresizingMaskIntoConstraints = NO;
    webSearchBtn.bezelStyle = NSBezelStyleRounded;
    [topBar addSubview:webSearchBtn];

    // 5. WebView
    self.webView = [[WKWebView alloc] init];
    self.webView.translatesAutoresizingMaskIntoConstraints = NO;
    self.webView.navigationDelegate = self;
    [contentView addSubview:self.webView];

    // 6. Налаштовуємо Auto Layout (обмеження)
    [NSLayoutConstraint activateConstraints:@[
        // TopBar прилипає до верху, лівого та правого краю
        [topBar.topAnchor constraintEqualToAnchor:contentView.topAnchor],
        [topBar.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor],
        [topBar.trailingAnchor constraintEqualToAnchor:contentView.trailingAnchor],
        [topBar.heightAnchor constraintEqualToConstant:60],

        // SearchField розтягується всередині TopBar
        [self.searchField.leadingAnchor constraintEqualToAnchor:topBar.leadingAnchor constant:20],
        [self.searchField.centerYAnchor constraintEqualToAnchor:topBar.centerYAnchor],
        
        // Кнопка "Локально"
        [localSearchBtn.leadingAnchor constraintEqualToAnchor:self.searchField.trailingAnchor constant:10],
        [localSearchBtn.centerYAnchor constraintEqualToAnchor:topBar.centerYAnchor],

        // Кнопка "В Інтернеті"
        [webSearchBtn.leadingAnchor constraintEqualToAnchor:localSearchBtn.trailingAnchor constant:10],
        [webSearchBtn.trailingAnchor constraintEqualToAnchor:topBar.trailingAnchor constant:-20],
        [webSearchBtn.centerYAnchor constraintEqualToAnchor:topBar.centerYAnchor],

        // WebView займає весь простір під TopBar
        [self.webView.topAnchor constraintEqualToAnchor:topBar.bottomAnchor],
        [self.webView.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor],
        [self.webView.trailingAnchor constraintEqualToAnchor:contentView.trailingAnchor],
        [self.webView.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor]
    ]];

    // Початковий екран
    [self showWelcomeScreen];
    [self.window makeKeyAndOrderFront:nil];
}

- (void)showWelcomeScreen {
    std::string html = R"(
        <html><head><meta charset='UTF-8'>
        <style>
            body { font-family: -apple-system, sans-serif; background: #1e1e2e; color: white; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
            h1 { font-size: 40px; background: linear-gradient(90deg, #89b4fa, #b4befe); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
        </style>
        </head><body><h1>Ласкаво просимо до HexBrow</h1></body></html>
    )";
    [self.webView loadHTMLString:[NSString stringWithUTF8String:html.c_str()] baseURL:nil];
}

// Обробка натискання Enter
- (void)controlTextDidEndEditing:(NSNotification *)obj {
    NSString *input = self.searchField.stringValue;
    if ([input hasPrefix:@"http://"] || [input hasPrefix:@"https://"]) {
        [self.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:input]]];
    } else {
        [self searchLocal:nil];
    }
}

// ЛОКАЛЬНИЙ ПОШУК (SQLite)
- (void)searchLocal:(id)sender {
    std::string query = [self.searchField.stringValue UTF8String];
    
    try {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString *dbPath = [[paths firstObject] stringByAppendingPathComponent:@"products.db"];
        
        DatabaseManager db([dbPath UTF8String]);
        db.initialize(); 
        auto products = db.searchProducts(query);

        // Сучасний HTML/CSS дизайн (Grid/Flexbox)
        std::string html = R"(
            <html><head><meta charset='UTF-8'>
            <style>
                body { font-family: -apple-system, sans-serif; padding: 30px; background: #f0f2f5; }
                h1 { color: #333; }
                .grid { display: flex; flex-wrap: wrap; gap: 20px; }
                .card { background: white; border-radius: 12px; padding: 20px; width: 300px; box-shadow: 0 4px 12px rgba(0,0,0,0.05); transition: transform 0.2s; }
                .card:hover { transform: translateY(-5px); box-shadow: 0 8px 24px rgba(0,0,0,0.1); }
                .card h2 { font-size: 18px; color: #1a1a1a; margin-top: 0; }
                .card .brand { color: #007aff; font-weight: bold; font-size: 14px; margin-bottom: 10px; }
                .card p { color: #666; font-size: 14px; line-height: 1.5; }
            </style>
            </head><body>
        )";
        html += "<h1>Результати пошуку: " + query + "</h1><div class='grid'>";

        if (products.empty()) {
            html += "<p>Товарів не знайдено.</p>";
        } else {
            for (const auto& p : products) {
                html += "<div class='card'>";
                html += "<div class='brand'>" + p.manufacturer + "</div>";
                html += "<h2>" + p.name + "</h2>";
                html += "<p><b>Код:</b> " + p.code + "</p>";
                html += "<p>" + p.description + "</p>";
                html += "</div>";
            }
        }
        html += "</div></body></html>";
        [self.webView loadHTMLString:[NSString stringWithUTF8String:html.c_str()] baseURL:nil];

    } catch (const std::exception& e) {
        [self.webView loadHTMLString:[NSString stringWithFormat:@"<h1>Помилка: %s</h1>", e.what()] baseURL:nil];
    }
}

// ПОШУК В ІНТЕРНЕТІ (libcurl + JSON)
- (void)searchInternet:(id)sender {
    NSString *query = self.searchField.stringValue;
    if (query.length == 0) return;

    [self.webView loadHTMLString:@"<html><body style='font-family:-apple-system; display:flex; justify-content:center; align-items:center; height:100vh;'><h1>Шукаємо в інтернеті... 🌐</h1></body></html>" baseURL:nil];

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        std::vector<Product> products = NetworkManager::fetchInternetProducts([query UTF8String]);

        std::string html = R"(
            <html><head><meta charset='UTF-8'>
            <style>
                body { font-family: -apple-system, sans-serif; padding: 30px; background: #eef2f5; }
                h1 { color: #333; }
                .grid { display: flex; flex-wrap: wrap; gap: 20px; }
                .card { background: white; border-radius: 12px; padding: 0; width: 280px; overflow: hidden; box-shadow: 0 4px 12px rgba(0,0,0,0.08); transition: transform 0.2s; }
                .card:hover { transform: translateY(-5px); box-shadow: 0 8px 24px rgba(0,0,0,0.15); }
                .card img { width: 100%; height: 180px; object-fit: cover; }
                .card-content { padding: 15px; }
                .card h2 { font-size: 16px; color: #1a1a1a; margin: 0 0 10px 0; }
                .card .price { color: #e63946; font-weight: bold; font-size: 18px; margin-bottom: 10px; }
                .card .brand { color: #666; font-size: 12px; text-transform: uppercase; margin-bottom: 5px; }
                .card a { display: block; text-align: center; background: #007aff; color: white; text-decoration: none; padding: 10px; border-radius: 6px; margin-top: 10px; font-weight: bold; }
                .card a:hover { background: #0056b3; }
            </style>
            </head><body>
        )";
        html += "<h1>Результати з інтернету: " + std::string([query UTF8String]) + "</h1><div class='grid'>";

        if (products.empty()) {
            html += "<p>Нічого не знайдено в мережі.</p>";
        } else {
            for (const auto& p : products) {
                html += "<div class='card'>";
                // Використовуємо зображення з API (передаємо його через NetworkManager)
                html += "<img src='" + p.imageUrl + "' alt='product'>";
                html += "<div class='card-content'>";
                html += "<div class='brand'>" + p.manufacturer + "</div>";
                html += "<h2>" + p.name + "</h2>";
                html += "<div class='price'>$" + p.price + "</div>";
                html += "<a href='" + p.link + "' target='_blank'>Купити / Переглянути</a>";
                html += "</div></div>";
            }
        }
        html += "</div></body></html>";

        dispatch_async(dispatch_get_main_queue(), ^{
            [self.webView loadHTMLString:[NSString stringWithUTF8String:html.c_str()] baseURL:nil];
        });
    });
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}