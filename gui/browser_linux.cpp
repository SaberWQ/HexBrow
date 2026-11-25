/*
 * ============================================================================
 * HexBrow — Linux Native GUI (GTK3 + WebKitGTK)
 * ============================================================================
 *
 * УКР: Нативне вікно для Linux. Використовує системні GTK3 та WebKitGTK.
 *      Запускає web_server через fork() і показує http://127.0.0.1:8080.
 *      Ніяких сторонніх бібліотек — лише пакети з репозиторію дистрибутива.
 *
 * ENG: Native Linux window. Uses system GTK3 and WebKitGTK.
 *      Forks web_server and loads http://127.0.0.1:8080.
 *      No third-party libs — only distro packages.
 * ============================================================================
 */

 #include <gtk/gtk.h>
 #include <webkit2/webkit2.h>
 #include <sys/types.h>
 #include <sys/wait.h>
 #include <unistd.h>
 #include <signal.h>
 #include <iostream>
 
 /* УКР: PID дочірнього процесу web_server */
 static pid_t g_server_pid = -1;
 
 /*
  * УКР: Зупиняє дочірній процес, якщо він живий.
  * ENG: Terminates the child process if alive.
  */
 static void stopServer() {
     if (g_server_pid > 0) {
         kill(g_server_pid, SIGTERM);                        // Graceful shutdown
         waitpid(g_server_pid, nullptr, 0);                  // Reap zombie
         g_server_pid = -1;
     }
 }
 
 /*
  * УКР: Обробник закриття вікна.
  * ENG: Window destroy handler.
  */
 static void onWindowDestroy(GtkWidget* widget, gpointer data) {
     stopServer();
     gtk_main_quit();
 }
 
 /*
  * УКР: Головна функція.
  * ENG: Main entry point.
  */
 int main(int argc, char* argv[]) {
     gtk_init(&argc, &argv);                                  // Init GTK
 
     /* Fork web_server */
     g_server_pid = fork();
     if (g_server_pid == 0) {
         execl("./web_server", "web_server", "--serve", "8080", (char*)nullptr);
         perror("execl");                                    // Only on error
         _exit(1);
     } else if (g_server_pid < 0) {
         perror("fork");
         return 1;
     }
 
     usleep(500000);                                         // Wait 0.5s for server
 
     /* Create window */
     GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
     gtk_window_set_title(GTK_WINDOW(window), "HexBrow");
     gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);
     g_signal_connect(window, "destroy", G_CALLBACK(onWindowDestroy), nullptr);
 
     /* Create WebView */
     GtkWidget* webView = webkit_web_view_new();
     gtk_container_add(GTK_CONTAINER(window), webView);
     webkit_web_view_load_uri(WEBKIT_WEB_VIEW(webView), "http://127.0.0.1:8080");
 
     /* Show + event loop */
     gtk_widget_show_all(window);
     gtk_main();
 
     stopServer();
     return 0;
 }