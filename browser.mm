#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface AppDelegate : NSObject <NSApplicationDelegate, WKNavigationDelegate>
@property (strong, nonatomic) NSWindow *window;
@property (strong, nonatomic) WKWebView *webView;
@property (strong, nonatomic) NSTask *serverTask;
@property (strong, nonatomic) NSProgressIndicator *spinner;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    [self startServer];
    
    NSRect frame = NSMakeRect(0, 0, 1280, 800);
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    [self.window setTitle:@"HexBrow — Твій Розумний Браузер"];
    [self.window center];
    self.window.minSize = NSMakeSize(800, 600);

    self.webView = [[WKWebView alloc] initWithFrame:self.window.contentView.bounds];
    self.webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.webView.navigationDelegate = self; // Додаємо делегат для обробки помилок
    [self.window.contentView addSubview:self.webView];

    self.spinner = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0, 0, 40, 40)];
    self.spinner.style = NSProgressIndicatorStyleSpinning;
    
    NSRect windowRect = self.window.contentView.bounds;
    NSRect spinnerRect = self.spinner.frame;
    spinnerRect.origin.x = (windowRect.size.width - spinnerRect.size.width) / 2.0;
    spinnerRect.origin.y = (windowRect.size.height - spinnerRect.size.height) / 2.0;
    self.spinner.frame = spinnerRect;
    
    self.spinner.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin | NSViewMaxYMargin;
    [self.spinner startAnimation:nil];
    [self.window.contentView addSubview:self.spinner];

    // Перша спроба завантаження
    [self loadPage];

    [self.window makeKeyAndOrderFront:nil];
}

- (void)loadPage {
    NSURL *url = [NSURL URLWithString:@"http://127.0.0.1:8080/?t=12345"];
    NSURLRequest *request = [NSURLRequest requestWithURL:url cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:5.0];
    [self.webView loadRequest:request];
}

// === ОБРОБКА ПОМИЛОК ЗАВАНТАЖЕННЯ ===
- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    // Якщо сервер ще не піднявся (Connection Refused), чекаємо 1 секунду і пробуємо знову
    NSLog(@"Помилка завантаження: %@. Спробуємо ще раз...", error.localizedDescription);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self loadPage];
    });
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    [self.spinner stopAnimation:nil];
    [self.spinner removeFromSuperview];
}

- (void)startServer {
    NSString *appPath = [[NSBundle mainBundle] bundlePath];
    NSString *projectPath = [appPath stringByDeletingLastPathComponent];
    NSString *serverPath = [projectPath stringByAppendingPathComponent:@"web_server"];
    
    NSLog(@"Шукаємо сервер: %@", serverPath);

    if (![[NSFileManager defaultManager] fileExistsAtPath:serverPath]) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Помилка запуску"];
        [alert setInformativeText:@"Файл web_server не знайдено. Виконайте 'make' у терміналі."];
        [alert setAlertStyle:NSAlertStyleCritical];
        [alert runModal];
        return;
    }

    NSTask *killTask = [[NSTask alloc] init];
    [killTask setLaunchPath:@"/usr/bin/killall"];
    [killTask setArguments:@[@"web_server"]];
    @try { [killTask launch]; [killTask waitUntilExit]; } @catch (NSException *e) {}

    self.serverTask = [[NSTask alloc] init];
    [self.serverTask setLaunchPath:serverPath];
    [self.serverTask setArguments:@[@"--serve", @"8080"]];
    
    NSPipe *pipe = [NSPipe pipe];
    [self.serverTask setStandardOutput:pipe];
    [self.serverTask setStandardError:pipe];

    @try { [self.serverTask launch]; } @catch (NSException *exception) { NSLog(@"ПОМИЛКА: %@", exception); }
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    if (self.serverTask && [self.serverTask isRunning]) {
        [self.serverTask terminate];
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender { return YES; }

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        (void)argc; (void)argv;
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}