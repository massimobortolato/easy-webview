# easy-webview

A small C++ library for building desktop apps with an HTML/JS front end and a C++ back end, with no local HTTP server and no open ports.

easy-webview opens a native window with an embedded [Microsoft Edge WebView2](https://learn.microsoft.com/microsoft-edge/webview2/) browser. Requests from the page to `https://app.example/...` are intercepted inside the process and routed to your C++ handlers or to a folder of static files. You write the routes the way you would for a small web framework, and the front end uses plain `fetch()`.

```cpp
#include "easy_webview.h"

int main()
{
    easy_webview::webview w("Hello");
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
- **Static file serving:** from a folder on disk or from files embedded in the executable. Content types are set from the file extension, `/` maps to `index.html`, and paths that would leave the static root are rejected.
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

```cpp
webview(const std::string &title, bool debug = false);
```

Creates the window with the given title. The `debug` flag controls the WebView2 developer tools. When it is `false` (the default), the developer tools are disabled, so F12, Ctrl+Shift+I and the "Inspect" context menu entry do nothing. Pass `true` during development to turn them on:

```cpp
easy_webview::webview w("Hello", true);   // developer tools enabled
```

From the C API, use `ew_create_debug(title, debug)`, where a nonzero `debug` enables the developer tools. `ew_create(title)` keeps them disabled:

```c
ew_webview *w = ew_create_debug("Hello", 1);   // developer tools enabled
```

#### Permissions

Permission requests from the page (camera, microphone, geolocation, notifications and so on) are **denied by default**. To grant some, pass a table as the third argument. Each entry maps a `permission` to `permission_state::allow`, `deny` or `ask` (show WebView2's own prompt):

```cpp
using easy_webview::permission;
using easy_webview::permission_state;

easy_webview::webview w("Hello", false, {
    {permission::camera, permission_state::allow},
    {permission::microphone, permission_state::ask},
});
```

The table applies only to pages served from `https://app.example`, the app's own front end. Pages from any other site, for example after `navigate()` or a link, are always denied, so a grant can't leak to remote content. Kinds missing from the table are denied as well.

Windows privacy settings still apply: if desktop apps aren't allowed to use the camera, `getUserMedia()` fails even when the table says `allow`.

From the C API, use `ew_create_with_permissions`:

```c
ew_permission_rule rules[] = {
    {EW_PERMISSION_CAMERA, EW_PERMISSION_ALLOW},
    {EW_PERMISSION_MICROPHONE, EW_PERMISSION_ASK},
};
ew_webview *w = ew_create_with_permissions("Hello", 0, rules, 2);
```

| Method | Description |
| --- | --- |
| `set_title(const std::string&)` | Sets the window title. |
| `set_size(int width, int height)` | Sets the window size. |
| `get / post / put / del / patch / head(pattern, handler)` | Registers a route for that HTTP method. |
| `serve_static(const std::filesystem::path&)` | Serves files from a directory for `GET`/`HEAD` requests that no route matched. |
| `serve_static(const std::map<std::string, std::string_view>&)` | Serves in-memory files (for example, assets embedded in the executable) for `GET`/`HEAD` requests that no route matched. Keys are URL paths such as `"/index.html"`. |
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

Routes are checked in the order they were registered, and the first match wins. A request that matches no route goes to the static files if it is a `GET` or `HEAD`: the embedded files are checked first, then the static folder. Anything left over gets a `404`.

### Embedded static files

The second `serve_static()` overload takes a map from URL paths to file contents, so the front end can ship inside the executable instead of in a folder next to it:

```cpp
static const std::map<std::string, std::string_view> files = {
    {"/index.html", "<!doctype html><h1>Hello</h1>"},
    {"/app.js", "console.log('hi');"},
};

w.serve_static(files);   // "/" serves "/index.html"
```

The map is copied, but the `std::string_view` contents are not, so the data they point to must outlive the window (string literals and static arrays are fine).

To embed a whole directory, use the `easy_webview_embed_dir` CMake helper. It generates a header declaring `extern const std::map<std::string, std::string_view> <name>;`, with one entry per file, and regenerates it when the files change:

```cmake
include("${easy-webview_SOURCE_DIR}/cmake/embed_dir.cmake")
easy_webview_embed_dir(my-app my_static "${CMAKE_CURRENT_SOURCE_DIR}/static")
```

```cpp
#include "my_static.h"

w.serve_static(my_static);
```

## How it works

`run()` points the WebView at `https://app.example/` and registers a WebView2 resource filter for `https://app.example/*`. Every matching request (page loads, `<script>`/`<link>` tags and `fetch()` calls) is handed to easy-webview. It parses the method, path, query and body, dispatches the request to your handler or the static file server, and returns the result as a WebView2 response. No network traffic is involved.

## Acknowledgements

easy-webview builds on these projects:

- [Microsoft Edge WebView2 SDK](https://www.nuget.org/packages/Microsoft.Web.WebView2) headers
- [Bootstrap](https://getbootstrap.com/) (used only by the example)

## License

easy-webview is released under the [BSD 3-Clause License](LICENSE).

It embeds third-party code that stays under its own license: the WebView2 SDK headers (BSD-style). The example also ships Bootstrap (MIT). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the full texts.
