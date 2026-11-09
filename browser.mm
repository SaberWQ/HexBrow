#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#include "src/DatabaseManager.h" 
#include "src/NetworkManager.h"
#include <string>

@class BrowserWindowController;

// ==========================================
// 1. ОГОЛОШЕННЯ КЛАСІВ
// ==========================================
@interface LoginWindowController : NSWindowController
@property (strong, nonatomic) NSTextField *userField;
@property (strong, nonatomic) NSSecureTextField *passField;
@end

@interface BrowserWindowController : NSWindowController <WKNavigationDelegate, NSSearchFieldDelegate>
@property (strong, nonatomic) NSView *topBar;
@property (strong, nonatomic) NSSearchField *searchField;
@property (strong, nonatomic) NSButton *backBtn;
@property (strong, nonatomic) NSButton *forwardBtn;
@property (strong, nonatomic) NSButton *reloadBtn;
@property (strong, nonatomic) NSView *tabBarContainer;
@property (strong, nonatomic) NSView *webViewContainer;
@property (strong, nonatomic) NSMutableArray<WKWebView *> *webViews;
@property (strong, nonatomic) NSMutableArray<NSButton *> *tabButtons;
@property (nonatomic) NSInteger activeTabIndex;
@end

static BrowserWindowController *mainBrowserWindow = nil;

// ==========================================
// 2. ВІКНО АВТОРИЗАЦІЇ
// ==========================================
@implementation LoginWindowController
- (instancetype)init {
    NSRect frame = NSMakeRect(0, 0, 400, 300);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    [win setTitle:@"Вхід у HexBrow"];
    [win center];
    self = [super initWithWindow:win];
    if (self) {
        NSView *view = win.contentView;
        self.userField = [[NSTextField alloc] initWithFrame:NSMakeRect(50, 180, 300, 30)];
        [self.userField setPlaceholderString:@"Логін"];
        [view addSubview:self.userField];

        self.passField = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(50, 130, 300, 30)];
        [self.passField setPlaceholderString:@"Пароль"];
        [view addSubview:self.passField];

        NSButton *loginBtn = [[NSButton alloc] initWithFrame:NSMakeRect(50, 70, 140, 40)];
        [loginBtn setTitle:@"Увійти"];
        [loginBtn setTarget:self];
        [loginBtn setAction:@selector(loginAction:)];
        [view addSubview:loginBtn];

        NSButton *regBtn = [[NSButton alloc] initWithFrame:NSMakeRect(210, 70, 140, 40)];
        [regBtn setTitle:@"Реєстрація"];
        [regBtn setTarget:self];
        [regBtn setAction:@selector(registerAction:)];
        [view addSubview:regBtn];
    }
    return self;
}

- (void)loginAction:(id)sender {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    std::string dbPath = [[[paths firstObject] stringByAppendingPathComponent:@"products.db"] UTF8String];
    DatabaseManager db(dbPath);
    db.initialize();

    if (db.loginUser([self.userField.stringValue UTF8String], [self.passField.stringValue UTF8String])) {
        [self.window close];
        mainBrowserWindow = [[BrowserWindowController alloc] init];
        [mainBrowserWindow showWindow:nil];
    } else {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Помилка"];
        [alert setInformativeText:@"Невірний логін або пароль"];
        [alert runModal];
    }
}

- (void)registerAction:(id)sender {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    std::string dbPath = [[[paths firstObject] stringByAppendingPathComponent:@"products.db"] UTF8String];
    DatabaseManager db(dbPath);
    db.initialize();

    if (db.registerUser([self.userField.stringValue UTF8String], [self.passField.stringValue UTF8String])) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Успіх"];
        [alert setInformativeText:@"Користувача створено! Тепер увійдіть."];
        [alert runModal];
    } else {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Помилка"];
        [alert setInformativeText:@"Користувач вже існує"];
        [alert runModal];
    }
}
@end

// ==========================================
// 3. ГОЛОВНЕ ВІКНО БРАУЗЕРА
// ==========================================
@implementation BrowserWindowController

- (instancetype)init {
    NSRect frame = NSMakeRect(0, 0, 1200, 800);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable) backing:NSBackingStoreBuffered defer:NO];
    [win setTitle:@"HexBrow - Твій Розумний Браузер"];
    [win center];
    self = [super initWithWindow:win];
    
    if (self) {
        self.webViews = [NSMutableArray array];
        self.tabButtons = [NSMutableArray array];
        self.activeTabIndex = -1;
        
        NSView *content = win.contentView;
        
        // Панель вкладок
        self.tabBarContainer = [[NSView alloc] init];
        self.tabBarContainer.translatesAutoresizingMaskIntoConstraints = NO;
        self.tabBarContainer.wantsLayer = YES;
        self.tabBarContainer.layer.backgroundColor = [NSColor windowBackgroundColor].CGColor;
        [content addSubview:self.tabBarContainer];
        
        // Панель навігації
        self.topBar = [[NSView alloc] init];
        self.topBar.translatesAutoresizingMaskIntoConstraints = NO;
        [content addSubview:self.topBar];
        
        self.backBtn = [NSButton buttonWithTitle:@"◀" target:self action:@selector(goBack:)];
        self.backBtn.translatesAutoresizingMaskIntoConstraints = NO;
        self.backBtn.enabled = NO;
        [self.topBar addSubview:self.backBtn];
        
        self.forwardBtn = [NSButton buttonWithTitle:@"▶" target:self action:@selector(goForward:)];
        self.forwardBtn.translatesAutoresizingMaskIntoConstraints = NO;
        self.forwardBtn.enabled = NO;
        [self.topBar addSubview:self.forwardBtn];
        
        self.reloadBtn = [NSButton buttonWithTitle:@"⟳" target:self action:@selector(reloadPage:)];
        self.reloadBtn.translatesAutoresizingMaskIntoConstraints = NO;
        [self.topBar addSubview:self.reloadBtn];
        
        self.searchField = [[NSSearchField alloc] init];
        self.searchField.translatesAutoresizingMaskIntoConstraints = NO;
        self.searchField.delegate = self;
        self.searchField.placeholderString = @"Введіть запит або URL...";
        [self.topBar addSubview:self.searchField];
        
        NSButton *localBtn = [NSButton buttonWithTitle:@"Локально" target:self action:@selector(searchLocal:)];
        localBtn.translatesAutoresizingMaskIntoConstraints = NO;
        [self.topBar addSubview:localBtn];
        
        NSButton *webBtn = [NSButton buttonWithTitle:@"В Інтернеті" target:self action:@selector(searchInternet:)];
        webBtn.translatesAutoresizingMaskIntoConstraints = NO;
        [self.topBar addSubview:webBtn];
        
        // Кнопка "+"
        NSButton *newTabBtn = [NSButton buttonWithTitle:@"+" target:self action:@selector(addNewTab:)];
        newTabBtn.translatesAutoresizingMaskIntoConstraints = NO;
        newTabBtn.font = [NSFont boldSystemFontOfSize:18];
        [self.topBar addSubview:newTabBtn];
        
        // Кнопка "✖" (Закрити вкладку)
        NSButton *closeTabBtn = [NSButton buttonWithTitle:@"✖" target:self action:@selector(closeTab:)];
        closeTabBtn.translatesAutoresizingMaskIntoConstraints = NO;
        closeTabBtn.font = [NSFont boldSystemFontOfSize:14];
        [self.topBar addSubview:closeTabBtn];
        
        // Контейнер для веб-сторінок
        self.webViewContainer = [[NSView alloc] init];
        self.webViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
        [content addSubview:self.webViewContainer];
        
        [NSLayoutConstraint activateConstraints:@[
            [self.tabBarContainer.topAnchor constraintEqualToAnchor:content.topAnchor],
            [self.tabBarContainer.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
            [self.tabBarContainer.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
            [self.tabBarContainer.heightAnchor constraintEqualToConstant:35],
            
            [self.topBar.topAnchor constraintEqualToAnchor:self.tabBarContainer.bottomAnchor],
            [self.topBar.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
            [self.topBar.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
            [self.topBar.heightAnchor constraintEqualToConstant:50],
            
            [self.backBtn.leadingAnchor constraintEqualToAnchor:self.topBar.leadingAnchor constant:10],
            [self.backBtn.centerYAnchor constraintEqualToAnchor:self.topBar.centerYAnchor],
            [self.backBtn.widthAnchor constraintEqualToConstant:30],
            
            [self.forwardBtn.leadingAnchor constraintEqualToAnchor:self.backBtn.trailingAnchor constant:5],
            [self.forwardBtn.centerYAnchor constraintEqualToAnchor:self.topBar.centerYAnchor],
            [self.forwardBtn.widthAnchor constraintEqualToConstant:30],
            
            [self.reloadBtn.leadingAnchor constraintEqualToAnchor:self.forwardBtn.trailingAnchor constant:5],
            [self.reloadBtn.centerYAnchor constraintEqualToAnchor:self.topBar.centerYAnchor],
            [self.reloadBtn.widthAnchor constraintEqualToConstant:30],
            
            [self.searchField.leadingAnchor constraintEqualToAnchor:self.reloadBtn.trailingAnchor constant:10],
            [self.searchField.centerYAnchor constraintEqualToAnchor:self.topBar.centerYAnchor],
            
            [localBtn.leadingAnchor constraintEqualToAnchor:self.searchField.trailingAnchor constant:10],
            [localBtn.centerYAnchor constraintEqualToAnchor:self.topBar.centerYAnchor],
            
            [webBtn.leadingAnchor constraintEqualToAnchor:localBtn.trailingAnchor constant:5],
            [webBtn.centerYAnchor constraintEqualToAnchor:self.topBar.centerYAnchor],
            
            [newTabBtn.leadingAnchor constraintEqualToAnchor:webBtn.trailingAnchor constant:10],
            [newTabBtn.centerYAnchor constraintEqualToAnchor:self.topBar.centerYAnchor],
            [newTabBtn.widthAnchor constraintEqualToConstant:30],
            
            [closeTabBtn.leadingAnchor constraintEqualToAnchor:newTabBtn.trailingAnchor constant:5],
            [closeTabBtn.trailingAnchor constraintEqualToAnchor:self.topBar.trailingAnchor constant:-10],
            [closeTabBtn.centerYAnchor constraintEqualToAnchor:self.topBar.centerYAnchor],
            [closeTabBtn.widthAnchor constraintEqualToConstant:30],
            
            [self.webViewContainer.topAnchor constraintEqualToAnchor:self.topBar.bottomAnchor],
            [self.webViewContainer.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
            [self.webViewContainer.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
            [self.webViewContainer.bottomAnchor constraintEqualToAnchor:content.bottomAnchor]
        ]];
        
        [self addNewTab:nil];
    }
    return self;
}

- (void)addNewTab:(id)sender {
    WKWebView *webView = [[WKWebView alloc] initWithFrame:self.webViewContainer.bounds];
    webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    webView.navigationDelegate = self;
    
    [webView addObserver:self forKeyPath:@"canGoBack" options:NSKeyValueObservingOptionNew context:nil];
    [webView addObserver:self forKeyPath:@"canGoForward" options:NSKeyValueObservingOptionNew context:nil];
    [webView addObserver:self forKeyPath:@"URL" options:NSKeyValueObservingOptionNew context:nil];
    [webView addObserver:self forKeyPath:@"title" options:NSKeyValueObservingOptionNew context:nil];
    
    [self.webViews addObject:webView];
    [self.webViewContainer addSubview:webView];
    
    NSButton *tabBtn = [NSButton buttonWithTitle:@"Нова вкладка" target:self action:@selector(switchTab:)];
    tabBtn.bezelStyle = NSBezelStyleRounded;
    [self.tabButtons addObject:tabBtn];
    [self.tabBarContainer addSubview:tabBtn];
    
    [self switchTab:tabBtn];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://www.google.com"]]];
    [self layoutTabs];
}

- (void)closeTab:(id)sender {
    if (self.activeTabIndex < 0) return;
    
    NSInteger indexToRemove = self.activeTabIndex;
    WKWebView *webViewToRemove = self.webViews[indexToRemove];
    NSButton *buttonToRemove = self.tabButtons[indexToRemove];
    
    @try {
        [webViewToRemove removeObserver:self forKeyPath:@"canGoBack"];
        [webViewToRemove removeObserver:self forKeyPath:@"canGoForward"];
        [webViewToRemove removeObserver:self forKeyPath:@"URL"];
        [webViewToRemove removeObserver:self forKeyPath:@"title"];
    } @catch (NSException *exception) {}
    
    [webViewToRemove removeFromSuperview];
    [buttonToRemove removeFromSuperview];
    [self.webViews removeObjectAtIndex:indexToRemove];
    [self.tabButtons removeObjectAtIndex:indexToRemove];
    
    if (self.webViews.count == 0) {
        [self addNewTab:nil];
    } else {
        if (indexToRemove >= self.webViews.count) {
            indexToRemove = self.webViews.count - 1;
        }
        [self switchTab:self.tabButtons[indexToRemove]];
    }
    [self layoutTabs];
}

- (void)switchTab:(NSButton *)sender {
    NSInteger index = [self.tabButtons indexOfObject:sender];
    if (index == NSNotFound) return;
    
    for (WKWebView *wv in self.webViews) { wv.hidden = YES; }
    
    WKWebView *activeWebView = self.webViews[index];
    activeWebView.hidden = NO;
    self.activeTabIndex = index;
    
    self.searchField.stringValue = activeWebView.URL.absoluteString ?: @"";
    [self updateNavButtons:activeWebView];
}

- (void)updateNavButtons:(WKWebView *)webView {
    self.backBtn.enabled = webView.canGoBack;
    self.forwardBtn.enabled = webView.canGoForward;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context {
    if ([object isKindOfClass:[WKWebView class]]) {
        WKWebView *webView = (WKWebView *)object;
        if (self.activeTabIndex >= 0 && self.activeTabIndex < self.webViews.count && webView == self.webViews[self.activeTabIndex]) {
            [self updateNavButtons:webView];
            
            if ([keyPath isEqualToString:@"URL"] && webView.URL) {
                NSString *urlString = webView.URL.absoluteString;
                if (![urlString containsString:@"hexbrow.local"] && ![urlString isEqualToString:@"about:blank"]) {
                    self.searchField.stringValue = urlString;
                }
            }
            if ([keyPath isEqualToString:@"title"] && webView.title) {
                NSButton *tabBtn = self.tabButtons[self.activeTabIndex];
                if (webView.title.length > 15) {
                    tabBtn.title = [[webView.title substringToIndex:15] stringByAppendingString:@"..."];
                } else {
                    tabBtn.title = webView.title;
                }
                [self layoutTabs];
            }
        }
    }
}

- (void)layoutTabs {
    CGFloat xOffset = 10;
    for (NSButton *btn in self.tabButtons) {
        [btn sizeToFit];
        CGFloat width = MAX(btn.frame.size.width + 20, 100);
        if (width > 200) width = 200;
        btn.frame = NSMakeRect(xOffset, 5, width, 25);
        xOffset += width + 5;
    }
}

- (void)goBack:(id)sender { if(self.activeTabIndex >= 0) [self.webViews[self.activeTabIndex] goBack]; }
- (void)goForward:(id)sender { if(self.activeTabIndex >= 0) [self.webViews[self.activeTabIndex] goForward]; }
- (void)reloadPage:(id)sender { if(self.activeTabIndex >= 0) [self.webViews[self.activeTabIndex] reload]; }

- (void)controlTextDidEndEditing:(NSNotification *)obj {
    NSString *input = self.searchField.stringValue;
    if (self.activeTabIndex < 0 || input.length == 0) return;
    
    WKWebView *currentView = self.webViews[self.activeTabIndex];
    
    if ([input hasPrefix:@"http://"] || [input hasPrefix:@"https://"]) {
        [currentView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:input]]];
    } else {
        [self searchLocal:nil];
    }
}

- (void)searchLocal:(id)sender {
    if (self.activeTabIndex < 0) return;
    std::string query = [self.searchField.stringValue UTF8String];
    if (query.find("http") != std::string::npos) query = ""; 

    try {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        std::string dbPath = [[[paths firstObject] stringByAppendingPathComponent:@"products.db"] UTF8String];
        
        DatabaseManager db(dbPath);
        db.initialize(); 
        auto products = db.searchProducts(query);

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
        
        [self.webViews[self.activeTabIndex] loadHTMLString:[NSString stringWithUTF8String:html.c_str()] baseURL:nil];

    } catch (const std::exception& e) {
        [self.webViews[self.activeTabIndex] loadHTMLString:[NSString stringWithFormat:@"<h1>Помилка: %s</h1>", e.what()] baseURL:nil];
    }
}

- (void)searchInternet:(id)sender {
    if (self.activeTabIndex < 0) return;
    NSString *query = self.searchField.stringValue;
    if ([query containsString:@"http"]) query = @"";
    if (query.length == 0) return;

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
            if (self.activeTabIndex >= 0) {
                [self.webViews[self.activeTabIndex] loadHTMLString:[NSString stringWithUTF8String:html.c_str()] baseURL:nil];
            }
        });
    });
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    NSString *url = webView.URL.absoluteString;
    NSString *title = webView.title;
    if (url && ![url containsString:@"hexbrow.local"] && ![url isEqualToString:@"about:blank"]) {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        std::string dbPath = [[[paths firstObject] stringByAppendingPathComponent:@"products.db"] UTF8String];
        DatabaseManager db(dbPath);
        db.addHistory([url UTF8String], [title UTF8String]);
    }
}

- (void)dealloc {
    for (WKWebView *webView in self.webViews) {
        @try {
            [webView removeObserver:self forKeyPath:@"canGoBack"];
            [webView removeObserver:self forKeyPath:@"canGoForward"];
            [webView removeObserver:self forKeyPath:@"URL"];
            [webView removeObserver:self forKeyPath:@"title"];
        } @catch (NSException *exception) {}
    }
}
@end

// ==========================================
// 4. ГОЛОВНА ФУНКЦІЯ
// ==========================================
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        LoginWindowController *login = [[LoginWindowController alloc] init];
        [login showWindow:nil];
        [app run];
    }
    return 0;
}