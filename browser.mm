#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#include "src/DatabaseManager.h" 
#include "src/NetworkManager.h"
#include "src/HtmlGenerator.h"
#include <string>

@class BrowserWindowController;

// ==========================================
// 1. ОГОЛОШЕННЯ КЛАСІВ
// ==========================================
@interface LoginWindowController : NSWindowController
@property (strong, nonatomic) NSTextField *userField;
@property (strong, nonatomic) NSSecureTextField *passField;
@property (strong, nonatomic) NSButton *rememberCheckbox;
@end

@interface BrowserWindowController : NSWindowController <WKNavigationDelegate, NSSearchFieldDelegate, WKScriptMessageHandler, WKUIDelegate>
@property (nonatomic) int currentLimit;
@property (strong, nonatomic) NSString *currentQuery;
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
// ДОПОМІЖНА ФУНКЦІЯ ДЛЯ ШЛЯХУ ДО БД
// ==========================================
NSString* getProjectDbPath() {
    NSString *exePath = [[NSBundle mainBundle] executablePath];
    NSString *macOSDir = [exePath stringByDeletingLastPathComponent];
    NSString *contentsDir = [macOSDir stringByDeletingLastPathComponent];
    NSString *appDir = [contentsDir stringByDeletingLastPathComponent];
    NSString *rootDir = [appDir stringByDeletingLastPathComponent];
    return [rootDir stringByAppendingPathComponent:@"products.db"];
}

// ==========================================
// 2. ВІКНО АВТОРИЗАЦІЇ
// ==========================================
@implementation LoginWindowController
- (instancetype)init {
    NSRect frame = NSMakeRect(0, 0, 400, 350);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    [win setTitle:@"Вхід у HexBrow"];
    [win center];
    self = [super initWithWindow:win];
    if (self) {
        NSView *view = win.contentView;
        NSString *savedUser = [[NSUserDefaults standardUserDefaults] stringForKey:@"SavedUsername"];
        
        self.userField = [[NSTextField alloc] initWithFrame:NSMakeRect(50, 200, 300, 30)];
        [self.userField setPlaceholderString:@"Логін"];
        if (savedUser) [self.userField setStringValue:savedUser];
        [view addSubview:self.userField];

        self.passField = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(50, 150, 300, 30)];
        [self.passField setPlaceholderString:@"Пароль"];
        [view addSubview:self.passField];

        self.rememberCheckbox = [NSButton checkboxWithTitle:@"Запам'ятати мене" target:nil action:nil];
        [self.rememberCheckbox setFrame:NSMakeRect(50, 110, 200, 30)];
        if (savedUser) [self.rememberCheckbox setState:NSControlStateValueOn];
        [view addSubview:self.rememberCheckbox];

        NSButton *loginBtn = [[NSButton alloc] initWithFrame:NSMakeRect(50, 60, 140, 40)];
        [loginBtn setTitle:@"Увійти"];
        [loginBtn setTarget:self];
        [loginBtn setAction:@selector(loginAction:)];
        [view addSubview:loginBtn];

        NSButton *regBtn = [[NSButton alloc] initWithFrame:NSMakeRect(210, 60, 140, 40)];
        [regBtn setTitle:@"Реєстрація"];
        [regBtn setTarget:self];
        [regBtn setAction:@selector(registerAction:)];
        [view addSubview:regBtn];
    }
    return self;
}

- (void)loginAction:(id)sender {
    if (self.rememberCheckbox.state == NSControlStateValueOn) {
        [[NSUserDefaults standardUserDefaults] setObject:self.userField.stringValue forKey:@"SavedUsername"];
    } else {
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"SavedUsername"];
    }
    [[NSUserDefaults standardUserDefaults] synchronize];

    std::string dbPath = [getProjectDbPath() UTF8String];
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
    std::string dbPath = [getProjectDbPath() UTF8String];
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

        NSButton *catalogBtn = [NSButton buttonWithTitle:@"Каталог" target:self action:@selector(showCatalog:)];
        catalogBtn.translatesAutoresizingMaskIntoConstraints = NO;
        [self.topBar addSubview:catalogBtn];

        NSButton *favBtn = [NSButton buttonWithTitle:@"⭐ Обрані" target:self action:@selector(showFavorites:)];
        favBtn.translatesAutoresizingMaskIntoConstraints = NO;
        [self.topBar addSubview:favBtn];
        
        NSButton *webBtn = [NSButton buttonWithTitle:@"В Інтернеті" target:self action:@selector(searchInternet:)];
        webBtn.translatesAutoresizingMaskIntoConstraints = NO;
        [self.topBar addSubview:webBtn];
        
        NSButton *newTabBtn = [NSButton buttonWithTitle:@"+" target:self action:@selector(addNewTab:)];
        newTabBtn.translatesAutoresizingMaskIntoConstraints = NO;
        newTabBtn.font = [NSFont boldSystemFontOfSize:18];
        [self.topBar addSubview:newTabBtn];
        
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
            
            [catalogBtn.leadingAnchor constraintEqualToAnchor:localBtn.trailingAnchor constant:5],
            [catalogBtn.centerYAnchor constraintEqualToAnchor:self.topBar.centerYAnchor],

            [favBtn.leadingAnchor constraintEqualToAnchor:catalogBtn.trailingAnchor constant:5],
            [favBtn.centerYAnchor constraintEqualToAnchor:self.topBar.centerYAnchor],
            
            [webBtn.leadingAnchor constraintEqualToAnchor:favBtn.trailingAnchor constant:5],
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
    WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];
    WKUserContentController *userController = [[WKUserContentController alloc] init];
    
    // ДОДАНО ВСІ ТРИ ОБРОБНИКИ
    [userController addScriptMessageHandler:self name:@"saveToCatalog"];
    [userController addScriptMessageHandler:self name:@"toggleFavorite"]; 
    [userController addScriptMessageHandler:self name:@"loadMore"];
    
    config.userContentController = userController;

    WKWebView *webView = [[WKWebView alloc] initWithFrame:self.webViewContainer.bounds configuration:config];
    webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    webView.navigationDelegate = self;
    webView.UIDelegate = self; 
    
    [webView addObserver:self forKeyPath:@"canGoBack" options:NSKeyValueObservingOptionNew context:nil];
    [webView addObserver:self forKeyPath:@"canGoForward" options:NSKeyValueObservingOptionNew context:nil];
    [webView addObserver:self forKeyPath:@"URL" options:NSKeyValueObservingOptionNew context:nil];
    [webView addObserver:self forKeyPath:@"title" options:NSKeyValueObservingOptionNew context:nil];
    
    [self.webViews addObject:webView];
    [self.webViewContainer addSubview:webView];
    
    NSButton *tabBtn = [NSButton buttonWithTitle:@"Головна" target:self action:@selector(switchTab:)];
    tabBtn.bezelStyle = NSBezelStyleRounded;
    [self.tabButtons addObject:tabBtn];
    [self.tabBarContainer addSubview:tabBtn];
    
    [self switchTab:tabBtn];
    [self loadWelcomeScreen:webView];
    [self layoutTabs];
}

// Обробка кліків з JS
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message {
    if ([message.name isEqualToString:@"saveToCatalog"]) {
        NSDictionary *dict = message.body;
        Product p;
        p.name = [dict[@"name"] UTF8String];
        p.code = [dict[@"code"] UTF8String];
        p.manufacturer = [dict[@"manufacturer"] UTF8String];
        p.description = [dict[@"description"] UTF8String];
        p.imageUrl = [dict[@"imageUrl"] UTF8String];
        p.price = [dict[@"price"] UTF8String];
        p.link = [dict[@"link"] UTF8String];
        p.isFavorite = 0;
        
        std::string dbPath = [getProjectDbPath() UTF8String];
        DatabaseManager db(dbPath);
        db.initialize();
        
        if (db.addProduct(p)) {
            NSAlert *alert = [[NSAlert alloc] init];
            [alert setMessageText:@"Успіх"];
            [alert setInformativeText:@"Товар успішно збережено в каталог!"];
            [alert runModal];
        } else {
            NSAlert *alert = [[NSAlert alloc] init];
            [alert setMessageText:@"Увага"];
            [alert setInformativeText:@"Цей товар вже є у вашому каталозі."];
            [alert runModal];
        }
    } else if ([message.name isEqualToString:@"toggleFavorite"]) {
        NSDictionary *dict = message.body;
        int pId = [dict[@"id"] intValue];
        int currentStatus = [dict[@"status"] intValue];
        
        std::string dbPath = [getProjectDbPath() UTF8String];
        DatabaseManager db(dbPath);
        db.initialize();
        db.toggleFavorite(pId, currentStatus);
        
        [self.webViews[self.activeTabIndex] reload]; 
    } else if ([message.name isEqualToString:@"loadMore"]) {
        if (self.activeTabIndex < 0) return;
        
        self.currentLimit += 20; // Збільшуємо ліміт
        
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            std::vector<Product> products = NetworkManager::fetchInternetProducts([self.currentQuery UTF8String], 0, self.currentLimit);
            
            std::string title = self.currentQuery.length > 0 ? std::string([self.currentQuery UTF8String]) : "Топ товарів з інтернету";
            std::string html = HtmlGenerator::generatePage(products, "Результати з інтернету: " + title, true, false);
            
            dispatch_async(dispatch_get_main_queue(), ^{
                if (self.activeTabIndex >= 0) {
                    NSString *fakeUrl = [NSString stringWithFormat:@"http://hexbrow.local/internet/%@", [[NSUUID UUID] UUIDString]];
                    [self.webViews[self.activeTabIndex] loadHTMLString:[NSString stringWithUTF8String:html.c_str()] baseURL:[NSURL URLWithString:fakeUrl]];
                }
            });
        });
    }
}

// Обробка посилань
- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures {
    if (!navigationAction.targetFrame.isMainFrame) {
        [[NSWorkspace sharedWorkspace] openURL:navigationAction.request.URL];
    }
    return nil;
}

- (void)loadWelcomeScreen:(WKWebView *)webView {
    std::string html = R"(
        <html><head><meta charset='UTF-8'>
        <style>
            body { font-family: -apple-system, sans-serif; background: #f0f2f5; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; }
            h1 { font-size: 48px; color: #333; margin-bottom: 10px; background: linear-gradient(90deg, #007aff, #00c6ff); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
            p { font-size: 18px; color: #666; margin-bottom: 30px; }
            .search-box { background: white; border-radius: 30px; padding: 15px 25px; box-shadow: 0 4px 15px rgba(0,0,0,0.1); width: 500px; text-align: center; }
        </style>
        </head><body>
        <h1>Welcome to HexBrow</h1>
        <p>Ваш персональний помічник для пошуку товарів</p>
        <div class="search-box">
            <p>Використовуйте поле вище, щоб шукати локально або в інтернеті.</p>
            <p style="font-size: 14px; color: #007aff;">Або натисніть "Каталог", щоб переглянути всі товари.</p>
        </div>
        </body></html>
    )";
    NSString *fakeUrl = [NSString stringWithFormat:@"http://hexbrow.local/home/%@", [[NSUUID UUID] UUIDString]];
    [webView loadHTMLString:[NSString stringWithUTF8String:html.c_str()] baseURL:[NSURL URLWithString:fakeUrl]];
}

// ==========================================
// МЕТОДИ ПОШУКУ
// ==========================================

- (void)searchLocal:(id)sender {
    if (self.activeTabIndex < 0) return;
    std::string query = [self.searchField.stringValue UTF8String];
    if (query.find("http") != std::string::npos) query = ""; 

    if (query.empty()) {
        NSString *html = @"<html><body style='font-family:-apple-system; padding:30px;'><h1>Введіть запит для локального пошуку</h1><p>Або натисніть кнопку \"Каталог\", щоб побачити всі товари.</p></body></html>";
        NSString *fakeUrl = [NSString stringWithFormat:@"http://hexbrow.local/empty/%@", [[NSUUID UUID] UUIDString]];
        [self.webViews[self.activeTabIndex] loadHTMLString:html baseURL:[NSURL URLWithString:fakeUrl]];
        return;
    }

    try {
        std::string dbPath = [getProjectDbPath() UTF8String];
        DatabaseManager db(dbPath);
        db.initialize(); 
        auto products = db.searchProducts(query);
        
        std::string html = HtmlGenerator::generatePage(products, "Результати пошуку: " + query, false, false);
        NSString *fakeUrl = [NSString stringWithFormat:@"http://hexbrow.local/search/%@", [[NSUUID UUID] UUIDString]];
        [self.webViews[self.activeTabIndex] loadHTMLString:[NSString stringWithUTF8String:html.c_str()] baseURL:[NSURL URLWithString:fakeUrl]];

    } catch (const std::exception& e) {}
}

- (void)showCatalog:(id)sender {
    if (self.activeTabIndex < 0) return;
    [self.searchField setStringValue:@""]; 
    try {
        std::string dbPath = [getProjectDbPath() UTF8String];
        DatabaseManager db(dbPath);
        db.initialize(); 
        auto products = db.searchProducts("");
        
        std::string html = HtmlGenerator::generatePage(products, "Офлайн каталог товарів", false, false);
        NSString *fakeUrl = [NSString stringWithFormat:@"http://hexbrow.local/catalog/%@", [[NSUUID UUID] UUIDString]];
        [self.webViews[self.activeTabIndex] loadHTMLString:[NSString stringWithUTF8String:html.c_str()] baseURL:[NSURL URLWithString:fakeUrl]];
    } catch (const std::exception& e) {}
}

- (void)showFavorites:(id)sender {
    if (self.activeTabIndex < 0) return;
    [self.searchField setStringValue:@""]; 
    try {
        std::string dbPath = [getProjectDbPath() UTF8String];
        DatabaseManager db(dbPath);
        db.initialize(); 
        auto products = db.getFavorites();
        
        std::string html = HtmlGenerator::generatePage(products, "⭐ Обрані товари", false, false);
        NSString *fakeUrl = [NSString stringWithFormat:@"http://hexbrow.local/favorites/%@", [[NSUUID UUID] UUIDString]];
        [self.webViews[self.activeTabIndex] loadHTMLString:[NSString stringWithUTF8String:html.c_str()] baseURL:[NSURL URLWithString:fakeUrl]];
    } catch (const std::exception& e) {}
}

- (void)searchInternet:(id)sender {
    if (self.activeTabIndex < 0) return;
    NSString *query = self.searchField.stringValue;
    if ([query containsString:@"http"]) query = @"";

    // ЗБЕРІГАЄМО ЗАПИТ ТА ЛІМІТ
    self.currentQuery = query;
    self.currentLimit = 20;

    NSString *loadingHtml = @"<html><body style='font-family:-apple-system; display:flex; justify-content:center; align-items:center; height:100vh; background:#f0f2f5;'><h1 style='color:#333;'>Шукаємо в інтернеті... 🌐</h1></body></html>";
    NSString *loadingUrl = [NSString stringWithFormat:@"http://hexbrow.local/loading/%@", [[NSUUID UUID] UUIDString]];
    [self.webViews[self.activeTabIndex] loadHTMLString:loadingHtml baseURL:[NSURL URLWithString:loadingUrl]];

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        // ВИКОРИСТОВУЄМО ЛІМІТ
        std::vector<Product> products = NetworkManager::fetchInternetProducts([query UTF8String], 0, self.currentLimit);
        
        std::string title = query.length > 0 ? std::string([query UTF8String]) : "Топ товарів з інтернету";
        std::string html = HtmlGenerator::generatePage(products, "Результати з інтернету: " + title, true, false);

        dispatch_async(dispatch_get_main_queue(), ^{
            if (self.activeTabIndex >= 0) {
                NSString *fakeUrl = [NSString stringWithFormat:@"http://hexbrow.local/internet/%@", [[NSUUID UUID] UUIDString]];
                [self.webViews[self.activeTabIndex] loadHTMLString:[NSString stringWithUTF8String:html.c_str()] baseURL:[NSURL URLWithString:fakeUrl]];
            }
        });
    });
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
        [webViewToRemove.configuration.userContentController removeScriptMessageHandlerForName:@"saveToCatalog"];
        [webViewToRemove.configuration.userContentController removeScriptMessageHandlerForName:@"toggleFavorite"];
        [webViewToRemove.configuration.userContentController removeScriptMessageHandlerForName:@"loadMore"];
    } @catch (NSException *exception) {}
    
    [webViewToRemove removeFromSuperview];
    [buttonToRemove removeFromSuperview];
    [self.webViews removeObjectAtIndex:indexToRemove];
    [self.tabButtons removeObjectAtIndex:indexToRemove];
    
    if (self.webViews.count == 0) { 
        [self addNewTab:nil]; 
    } else {
        if (indexToRemove >= (NSInteger)self.webViews.count) indexToRemove = (NSInteger)self.webViews.count - 1;
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
        if (self.activeTabIndex >= 0 && self.activeTabIndex < (NSInteger)self.webViews.count && webView == self.webViews[self.activeTabIndex]) {
            [self updateNavButtons:webView];
            if ([keyPath isEqualToString:@"URL"] && webView.URL) {
                NSString *urlString = webView.URL.absoluteString;
                if (![urlString containsString:@"hexbrow.local"] && ![urlString isEqualToString:@"about:blank"]) {
                    self.searchField.stringValue = urlString;
                }
            }
            if ([keyPath isEqualToString:@"title"] && webView.title) {
                NSButton *tabBtn = self.tabButtons[self.activeTabIndex];
                tabBtn.title = (webView.title.length > 15) ? [[webView.title substringToIndex:15] stringByAppendingString:@"..."] : webView.title;
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

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    NSString *url = webView.URL.absoluteString;
    NSString *title = webView.title;
    if (url && ![url containsString:@"hexbrow.local"] && ![url isEqualToString:@"about:blank"]) {
        std::string dbPath = [getProjectDbPath() UTF8String];
        DatabaseManager db(dbPath);
        db.addHistory([url UTF8String], [title UTF8String]);
    }
    
    if (self.activeTabIndex >= 0 && self.activeTabIndex < (NSInteger)self.webViews.count && webView == self.webViews[self.activeTabIndex]) {
        self.backBtn.enabled = webView.canGoBack;
        self.forwardBtn.enabled = webView.canGoForward;
    }
}

- (void)dealloc {
    for (WKWebView *webView in self.webViews) {
        @try {
            [webView removeObserver:self forKeyPath:@"canGoBack"];
            [webView removeObserver:self forKeyPath:@"canGoForward"];
            [webView removeObserver:self forKeyPath:@"URL"];
            [webView removeObserver:self forKeyPath:@"title"];
            [webView.configuration.userContentController removeScriptMessageHandlerForName:@"saveToCatalog"];
            [webView.configuration.userContentController removeScriptMessageHandlerForName:@"toggleFavorite"];
            [webView.configuration.userContentController removeScriptMessageHandlerForName:@"loadMore"];
        } @catch (NSException *exception) {}
    }
}
@end

// ==========================================
// 4. ГОЛОВНА ФУНКЦІЯ
// ==========================================
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        (void)argc; (void)argv;
        NSApplication *app = [NSApplication sharedApplication];
        LoginWindowController *login = [[LoginWindowController alloc] init];
        [login showWindow:nil];
        [app run];
    }
    return 0;
}