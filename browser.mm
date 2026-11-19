#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

/*
 * УКР: Делегат додатку. Керує вікном, WebView та сервером.
 * ENG: App delegate. Manages window, WebView, and server.
 */
@interface AppDelegate : NSObject <NSApplicationDelegate, WKNavigationDelegate>
@property (strong, nonatomic) NSWindow *window;
@property (strong, nonatomic) WKWebView *webView;
@property (strong, nonatomic) NSTask *serverTask;
@property (strong, nonatomic) NSProgressIndicator *spinner;
@end

@implementation AppDelegate

/*
 * УКР: Створює вікно, WebView, спінер та запускає сервер.
 * ENG: Creates window, WebView, spinner, and starts server.
 */
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    [self startServer];                                         // Start server first

    NSRect frame = NSMakeRect(0, 0, 1280, 800);                 // Window frame
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    [self.window setTitle:@"HexBrow — Твій Розумний Браузер"];
    [self.window center];
    self.window.minSize = NSMakeSize(800, 600);

    self.webView = [[WKWebView alloc] initWithFrame:self.window.contentView.bounds];
    self.webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.webView.navigationDelegate = self;                     // For load errors
    [self.window.contentView addSubview:self.webView];

    self.spinner = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0, 0, 40, 40)];
    self.spinner.style = NSProgressIndicatorStyleSpinning;

    NSRect windowRect = self.window.contentView.bounds;         // Center spinner
    NSRect spinnerRect = self.spinner.frame;
    spinnerRect.origin.x = (windowRect.size.width - spinnerRect.size.width) / 2.0;
    spinnerRect.origin.y = (windowRect.size.height - spinnerRect.size.height) / 2.0;
    self.spinner.frame = spinnerRect;

    self.spinner.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin | NSViewMaxYMargin;
    [self.spinner startAnimation:nil];
    [self.window.contentView addSubview:self.spinner];

    [self loadPage];                                            // First load attempt
    [self.window makeKeyAndOrderFront:nil];
}

/*
 * УКР: Завантажує http://127.0.0.1:8080 (з антикешем).
 * ENG: Loads http://127.0.0.1:8080 (with anti-cache).
 */
- (void)loadPage {
    NSURL *url = [NSURL URLWithString:@"http://127.0.0.1:8080/?t=12345"]; // Anti-cache param
    NSURLRequest *request = [NSURLRequest requestWithURL:url cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:5.0];
    [self.webView loadRequest:request];
}

/*
 * УКР: Якщо сервер ще не готовий — повторюємо через 1 секунду.
 * ENG: If server is not ready yet — retry after 1 second.
 */
- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    NSLog(@"Load error: %@. Retrying...", error.localizedDescription);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self loadPage];                                        // Retry
    });
}

/*
 * УКР: Коли сторінка завантажена — ховаємо спінер.
 * ENG: When page loaded — hide spinner.
 */
- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    [self.spinner stopAnimation:nil];
    [self.spinner removeFromSuperview];
}

/*
 * УКР: Запускає web_server як дочірній процес. Спершу вбиває старий.
 * ENG: Starts web_server as a child process. Kills old one first.
 */
- (void)startServer {
    NSString *appPath = [[NSBundle mainBundle] bundlePath];     // .app path
    NSString *projectPath = [appPath stringByDeletingLastPathComponent];
    NSString *serverPath = [projectPath stringByAppendingPathComponent:@"web_server"];

    NSLog(@"Looking for server: %@", serverPath);

    if (![[NSFileManager defaultManager] fileExistsAtPath:serverPath]) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Launch error"];
        [alert setInformativeText:@"web_server not found. Run 'make' in terminal."];
        [alert setAlertStyle:NSAlertStyleCritical];
        [alert runModal];
        return;
    }

    NSTask *killTask = [[NSTask alloc] init];                   // Kill old server
    [killTask setLaunchPath:@"/usr/bin/killall"];
    [killTask setArguments:@[@"web_server"]];
    @try { [killTask launch]; [killTask waitUntilExit]; } @catch (NSException *e) {}

    self.serverTask = [[NSTask alloc] init];                    // Start new server
    [self.serverTask setLaunchPath:serverPath];
    [self.serverTask setArguments:@[@"--serve", @"8080"]];

    NSPipe *pipe = [NSPipe pipe];                               // Redirect output
    [self.serverTask setStandardOutput:pipe];
    [self.serverTask setStandardError:pipe];

    @try {
        [self.serverTask launch];
        NSLog(@"Server started with PID: %d", self.serverTask.processIdentifier);
    } @catch (NSException *exception) {
        NSLog(@"ERROR: %@", exception);
    }
}

/*
 * УКР: При закритті додатку — зупиняємо сервер.
 * ENG: On termination — stop server.
 */
- (void)applicationWillTerminate:(NSNotification *)notification {
    if (self.serverTask && [self.serverTask isRunning]) [self.serverTask terminate];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender { return YES; }

@end

/*
 * УКР: Точка входу.
 * ENG: Entry point.
 */
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        (void)argc; (void)argv;                                 // Suppress warnings
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}