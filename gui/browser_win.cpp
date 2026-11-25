/*
 * ============================================================================
 * HexBrow — Windows Native GUI (Win32 + WebView2)
 * ============================================================================
 *
 * УКР: Нативне вікно для Windows 10/11. Використовує Win32 API та Edge
 *      WebView2 (вбудований у систему). Запускає web_server.exe без вікна
 *      консолі та показує http://127.0.0.1:8080.
 *
 * ENG: Native Windows 10/11 window. Uses Win32 API and Edge WebView2
 *      (built into the OS). Starts web_server.exe without a console window
 *      and loads http://127.0.0.1:8080.
 *
 * ВИМОГИ / REQUIREMENTS:
 *   - Visual Studio 2022 (MSVC, C++17)
 *   - Windows 10/11 SDK (windows.h, wrl.h)
 *   - WebView2 SDK (NuGet: Microsoft.Web.WebView2)
 * ============================================================================
 */

 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
 #include <wrl.h>
 #include <WebView2.h>
 #include <string>
 #include <iostream>
 
 using namespace Microsoft::WRL;
 
 /* Globals */
 static HWND g_hwnd = nullptr;
 static ComPtr<ICoreWebView2Controller> g_controller;
 static ComPtr<ICoreWebView2> g_webview;
 static PROCESS_INFORMATION g_pi = {};
 
 /*
  * УКР: Запуск web_server.exe у фоні (без вікна консолі).
  * ENG: Launch web_server.exe in the background (no console window).
  */
 static void startServer() {
     STARTUPINFOW si = { sizeof(si) };
     si.dwFlags = STARTF_USESHOWWINDOW;
     si.wShowWindow = SW_HIDE;                               // Hide console
 
     std::wstring cmd = L"web_server.exe --serve 8080";
     CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                    CREATE_NO_WINDOW, nullptr, nullptr, &si, &g_pi);
     Sleep(500);                                             // Wait for server
 }
 
 /*
  * УКР: Зупинка дочірнього процесу.
  * ENG: Terminate the child process.
  */
 static void stopServer() {
     if (g_pi.hProcess) {
         TerminateProcess(g_pi.hProcess, 0);
         CloseHandle(g_pi.hProcess);
         CloseHandle(g_pi.hThread);
     }
 }
 
 /*
  * УКР: Callback після створення середовища WebView2.
  * ENG: Callback after WebView2 environment creation.
  */
 static HRESULT onEnvCreated(HRESULT res, ICoreWebView2Environment* env) {
     if (FAILED(res)) return res;
 
     return env->CreateCoreWebView2Controller(g_hwnd,
         Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
             [](HRESULT res, ICoreWebView2Controller* ctrl) -> HRESULT {
                 if (FAILED(res)) return res;
                 g_controller = ctrl;
                 g_controller->get_CoreWebView2(&g_webview);
 
                 RECT bounds;
                 GetClientRect(g_hwnd, &bounds);
                 g_controller->put_Bounds(bounds);
                 g_webview->Navigate(L"http://127.0.0.1:8080");
                 return S_OK;
             }).Get());
 }
 
 /*
  * УКР: Віконна процедура.
  * ENG: Window procedure.
  */
 static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
     switch (msg) {
     case WM_SIZE:
         if (g_controller) {
             RECT bounds;
             GetClientRect(hwnd, &bounds);
             g_controller->put_Bounds(bounds);
         }
         return 0;
     case WM_DESTROY:
         stopServer();
         PostQuitMessage(0);
         return 0;
     }
     return DefWindowProcW(hwnd, msg, wp, lp);
 }
 
 /*
  * УКР: Точка входу.
  * ENG: Entry point.
  */
 int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
     SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
     CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
 
     startServer();                                          // Start C++ server
 
     WNDCLASSW wc = {};
     wc.lpfnWndProc = wndProc;
     wc.hInstance = hInst;
     wc.lpszClassName = L"HexBrowWindow";
     wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
     RegisterClassW(&wc);
 
     g_hwnd = CreateWindowExW(0, L"HexBrowWindow", L"HexBrow",
         WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
         nullptr, nullptr, hInst, nullptr);
 
     CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
         Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
             onEnvCreated).Get());
 
     ShowWindow(g_hwnd, nShow);
     UpdateWindow(g_hwnd);
 
     MSG msg;
     while (GetMessageW(&msg, nullptr, 0, 0)) {
         TranslateMessage(&msg);
         DispatchMessageW(&msg);
     }
 
     CoUninitialize();
     return 0;
 }