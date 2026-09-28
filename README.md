# easy-webview

A small C++ library for building desktop apps with an HTML/JS front end and a C++ back end, with no local HTTP server and no open ports.

easy-webview opens a native window with an embedded [Microsoft Edge WebView2](https://learn.microsoft.com/microsoft-edge/webview2/) browser. Requests from the page to `http://app.local/...` are intercepted inside the process and routed to your C++ handlers or to a folder of static files. You write the routes the way you would for a small web framework, and the front end uses plain `fetch()`.

```cpp
#include "easy_webview.h"

int main()
{
    easy_webview::webview w;
    w.set_title("Hello");
    w.set_size(800, 600);

    w.serve_static("static");   // serves ./static/index.html at "/"

    w.get("/api/hello/:name", [](const easy_webview::request &req) {
        return easy_webview::response{200, "text/plain", "Hello, " + req.params.at("name") + "!"};
    });

    w.run();
}
```

```js
const text = await (await fetch("/api/hello/world")).text(); // "Hello, world!"
```

## Features

- **In-process routing:** requests are handled inside the process through WebView2's `WebResourceRequested` event, so no socket is opened and no port is exposed.
- **Express-style routes:** `get`, `post`, `put`, `del`, `patch` and `head`, with `:param` segments and `*` wildcards.
- **Query string and body parsing:** percent-decoded query parameters and the raw request body are passed to each handler.
- **Static file serving:** content types are set from the file extension, `/` maps to `index.html`, and paths that would leave the static root are rejected.
- **Small API:** one header and a single class.

## Requirements

- Windows 10 or 11 with the [WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/) (preinstalled on Windows 11 and on up-to-date Windows 10)
- A C++23 compiler (MSVC from Visual Studio 2022 is recommended)
- CMake 3.14 or newer

> **Note:** only Windows is supported for now. The routing layer is built on WebView2.

## Getting started

### Using CMake FetchContent

```cmake
cmake_minimum_required(VERSION 3.14)
project(my-app LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(
    easy-webview
    GIT_REPOSITORY https://github.com/massimobortolato/easy-webview.git
    GIT_TAG main
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(easy-webview)

add_executable(my-app main.cpp)
target_link_libraries(my-app PRIVATE easy-webview::easy-webview)
```

### Building from source

```sh
git clone https://github.com/massimobortolato/easy-webview.git
cd easy-webview
cmake -B build
cmake --build build --config Release
```

### Running the example

The [example/](example/) folder contains a small app with a Bootstrap front end that calls C++ routes:

```sh
cd example
cmake -B build
cmake --build build --config Release
```

Run the executable from the `example` directory so the relative `static` folder is found.

## API

### `easy_webview::webview`

| Method | Description |
| --- | --- |
| `set_title(const std::string&)` | Sets the window title. |
| `set_size(int width, int height)` | Sets the window size. |
| `get / post / put / del / patch / head(pattern, handler)` | Registers a route for that HTTP method. |
| `serve_static(const std::filesystem::path&)` | Serves files from a directory for `GET`/`HEAD` requests that no route matched. |
| `run()` | Shows the window, loads `/` and blocks until the window is closed. |

### Handlers

```cpp
using handler = std::function<response(const request &)>;

struct request {
    std::string method;                         // "GET", "POST", ...
    std::string path;                           // "/api/users/42"
    std::map<std::string, std::string> query;   // "?a=1"      -> query["a"]
    std::map<std::string, std::string> params;  // "/:id"      -> params["id"]
    std::string body;                           // raw request body
};

struct response {
    int status;                 // e.g. 200
    std::string content_type;   // e.g. "application/json"
    std::string body;
};
```

### Route patterns

| Pattern | Matches | `params` |
| --- | --- | --- |
| `/api/status` | `/api/status` | – |
| `/users/:id` | `/users/42` | `id = "42"` |
| `/files/*` | `/files/a/b/c.txt` | `splat = "a/b/c.txt"` |

Routes are checked in the order they were registered, and the first match wins. A request that matches no route goes to the static folder if it is a `GET` or `HEAD`. Anything left over gets a `404`.

## How it works

`run()` points the WebView at `http://app.local/` and registers a WebView2 resource filter for `http://app.local/*`. Every matching request (page loads, `<script>`/`<link>` tags and `fetch()` calls) is handed to easy-webview. It parses the method, path, query and body, dispatches the request to your handler or the static file server, and returns the result as a WebView2 response. No network traffic is involved.

## Acknowledgements

easy-webview builds on these projects:

- [webview/webview](https://github.com/webview/webview) (MIT License, © Serge Zaitsev, Steffen André Langnes)
- [Microsoft Edge WebView2 SDK](https://www.nuget.org/packages/Microsoft.Web.WebView2) headers
- [Bootstrap](https://getbootstrap.com/) (used only by the example)

## License

easy-webview is released under the [BSD 3-Clause License](LICENSE).

It embeds third-party code that stays under its own license: webview (MIT) and the WebView2 SDK headers (BSD-style). The example also ships Bootstrap (MIT). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the full texts.
