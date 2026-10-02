#include "easy_webview.h"
#include "example_static.h"
#include <windows.h>

using easy_webview::handler;
using easy_webview::request;
using easy_webview::response;
using easy_webview::webview;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
// int main()
{
    webview w("WebView2 Example");
    w.navigate("https://www.duckduckgo.com");

    webview w2("WebView2 Example 2");
    w2.serve_static(example_static);
    w2.get("/api/data", [](const request &req) { return response{200, "text/plain; charset=utf-8", "Here your data!"}; });
    w2.post("/ciao", [](const request &req) { return response{200, "text/plain; charset=utf-8", "Ciao!"}; });

    webview::run();
    return 0;
}
