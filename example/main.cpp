#include "easy_webview.h"

int main()
{
    using easy_webview::request;
    using easy_webview::response;
    using easy_webview::webview;

    webview w;
    w.set_title("Kiosk");
    w.set_size(800, 600);
    w.serve_static("static");
    w.get("/api/data", [](const request &req)
          { return response{200, "plain/text", "the data=Ciao!"}; });
    w.post("/ciao", [](const request &req)
          { return response{200, "text/html", "<h1>Ciao!</h1>"}; });
    w.run();
}
