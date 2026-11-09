#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

// Підключаємо C++ код!
#include "src/DatabaseManager.h" 
#include <string>
#include <iostream>

// Додаємо протокол WKNavigationDelegate для обробки помилок
@interface AppDelegate : NSObject <NSApplicationDelegate, NSTextFieldDelegate, WKNavigationDelegate>
@property (strong, nonatomic) NSWindow *window;
@property (strong, nonatomic) WKWebView *webView;
@property (strong, nonatomic) NSTextField *urlField;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    NSRect frame = NSMakeRect(0, 0, 1000, 700);
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    [self.window setTitle:@"Мій Власний Браузер Товарів (HexBrow)"];
    [self.window center];

    NSView *contentView = self.window.contentView;
    
    self.urlField = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 660, 880, 30)];
    [self.urlField setPlaceholderString:@"Введіть запит (напр. Dell) і натисніть Enter"];
    self.urlField.delegate = self;
    [contentView addSubview:self.urlField];

    NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(900, 660, 90, 30)];
    [button setTitle:@"Знайти"];
    [button setTarget:self];
    [button setAction:@selector(loadURL:)];
    [contentView addSubview:button];

    self.webView = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1000, 650)];
    self.webView.navigationDelegate = self; // Підключаємо обробник помилок
    [contentView addSubview:self.webView];

    [self loadURL:nil];
    [self.window makeKeyAndOrderFront:nil];
}

- (void)loadURL:(id)sender {
    NSString *input = [self.urlField stringValue];
    if (input.length == 0) return;

    // ПЕРЕВІРКА: Якщо ввели URL (починається з http або https) - йдемо в інтернет
    if ([input hasPrefix:@"http://"] || [input hasPrefix:@"https://"]) {
        NSURL *url = [NSURL URLWithString:input];
        NSURLRequest *request = [NSURLRequest requestWithURL:url];
        [self.webView loadRequest:request];
        return; // Виходимо, щоб не шукати в базі
    }

    // ІНАКШЕ: Шукаємо в нашій локальній базі даних
    std::string searchQuery = [input UTF8String];

    try {
        // Шлях до БД у папці "Документи"
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString *documentsDirectory = [paths firstObject];
        NSString *dbPath = [documentsDirectory stringByAppendingPathComponent:@"products.db"];
        
        DatabaseManager db([dbPath UTF8String]);
        db.initialize(); 
        auto products = db.searchProducts(searchQuery);

        std::string html = "<html><head><meta charset='UTF-8'><style>";
        html += "body { font-family: -apple-system, sans-serif; padding: 20px; background: #f4f4f9; }";
        html += ".product { background: white; border-radius: 8px; padding: 15px; margin-bottom: 15px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
        html += "h1 { color: #333; }";
        html += "</style></head><body>";
        html += "<h1>Результати пошуку: " + searchQuery + "</h1>";

        if (products.empty()) {
            html += "<p>Товарів не знайдено.</p>";
        } else {
            for (const auto& p : products) {
                html += "<div class='product'>";
                html += "<h2>" + p.name + "</h2>";
                html += "<p><b>Код:</b> " + p.code + "</p>";
                html += "<p><b>Виробник:</b> " + p.manufacturer + "</p>";
                html += "<p>" + p.description + "</p>";
                html += "</div>";
            }
        }
        html += "</body></html>";

        NSString *nsHtml = [NSString stringWithUTF8String:html.c_str()];
        [self.webView loadHTMLString:nsHtml baseURL:nil];

    } catch (const std::exception& e) {
        NSString *errorMsg = [NSString stringWithUTF8String:e.what()];
        NSString *errorHtml = [NSString stringWithFormat:@"<html><head><meta charset='UTF-8'></head><body><h1>Помилка БД</h1><p>%@</p></body></html>", errorMsg];
        [self.webView loadHTMLString:errorHtml baseURL:nil];
    }
}

- (void)controlTextDidEndEditing:(NSNotification *)obj {
    [self loadURL:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

// === НОВИЙ МЕТОД: Обробка помилок завантаження ===
- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    // Якщо сторінка не завантажилась, показуємо гарну помилку
    NSString *errorHtml = [NSString stringWithFormat:
        @"<html><body style='font-family: -apple-system, sans-serif; padding: 40px; background-color: #f5f5f5; color: #333;'>"
         "<h1 style='color: #d32f2f;'>Помилка завантаження</h1>"
         "<p>Браузер не зміг з'єднатися з локальним сервером (Apache).</p>"
         "<p><b>Технічна причина:</b> %@</p>"
         "<hr>"
         "<h3>Як це виправити?</h3>"
         "<ol>"
         "<li>Відкрийте термінал і запустіть сервер: <code>sudo apachectl start</code></li>"
         "<li>Перевірте, чи скопійовано скрипт: <code>sudo cp search.cgi /Library/WebServer/CGI-Executables/</code></li>"
         "<li>Перевірте, чи має скрипт права: <code>sudo chmod +x /Library/WebServer/CGI-Executables/search.cgi</code></li>"
         "</ol>"
         "</body></html>", error.localizedDescription];
    
    [self.webView loadHTMLString:errorHtml baseURL:nil];
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