#include "easy_webview.h"
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <list>
#include <memory>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <webview2.h>
#include <windows.h>
#include <wrl.h>

namespace fs = std::filesystem;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

using easy_webview::handler;
using easy_webview::request;
using easy_webview::response;
using easy_webview::webview;

namespace
{
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct route
{
    std::string method;
    std::vector<std::string> pattern;
    handler fn;

    bool matches(const std::string &method, const std::vector<std::string> &parts,
                 std::map<std::string, std::string> &params) const
    {
        if (this->method != method)
            return false;
        params.clear();
        for (size_t i = 0; i < this->pattern.size(); ++i)
        {
            const std::string &pat = this->pattern[i];
            if (pat == "*")
            {
                std::string rest;
                for (size_t j = i; j < parts.size(); ++j)
                    rest += (j > i ? "/" : "") + parts[j];
                params["splat"] = rest;
                return true;
            }
            if (i >= parts.size())
                return false;
            if (pat[0] == ':')
                params[pat.substr(1)] = parts[i];
            else if (pat != parts[i])
                return false;
        }
        return parts.size() == this->pattern.size();
    }
};

class webview_once
{
    webview_once()
    {
        if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
            return;

        WNDCLASSEXW wc = {sizeof(wc)};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"WebView2Window";
        RegisterClassExW(&wc);
    }
    ~webview_once() { CoUninitialize(); }

    std::list<HWND> _handles;

public:
    static webview_once &instance()
    {
        static webview_once instance;
        return instance;
    }
    static void register_handle(HWND hwnd) { instance()._handles.push_back(hwnd); }
    static void remove_handle(HWND hwnd)
    {
        auto &handles = instance()._handles;
        auto it = std::find(handles.begin(), handles.end(), hwnd);
        if (it != handles.end())
        {
            handles.erase(it);
        }
    }
    static bool has_handles() { return !instance()._handles.empty(); }
    static void run()
    {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
};

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // The controller pointer is passed to CreateWindowExW and stored in
    // GWLP_USERDATA.
    ComPtr<ICoreWebView2Controller> *controller;
    if (msg == WM_NCCREATE)
    {
        auto cs = reinterpret_cast<CREATESTRUCTW *>(lParam);
        controller = static_cast<ComPtr<ICoreWebView2Controller> *>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(controller));
    }
    else
    {
        controller = reinterpret_cast<ComPtr<ICoreWebView2Controller> *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    switch (msg)
    {
    case WM_SIZE:
        if (controller && *controller)
        {
            RECT bounds;
            GetClientRect(hwnd, &bounds);
            (*controller)->put_Bounds(bounds);
        }
        return 0;
    case WM_DESTROY:
        webview_once::remove_handle(hwnd);
        if (!webview_once::has_handles())
            PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ShowError(HWND hwnd, const wchar_t *what, HRESULT hr)
{
    wchar_t text[256];
    swprintf_s(text, L"%s (HRESULT 0x%08lX).", what, static_cast<unsigned long>(hr));
    MessageBoxW(hwnd, text, L"WebView2 error", MB_ICONERROR);
}
std::string read_stream(IStream *stream)
{
    std::string data;
    if (!stream)
        return data;
    char buf[4096];
    ULONG read = 0;
    while (SUCCEEDED(stream->Read(buf, sizeof(buf), &read)) && read > 0)
        data.append(buf, read);
    return data;
}

std::vector<std::string> split_path(const std::string &path)
{
    std::vector<std::string> parts;
    for (size_t i = 1; i <= path.size();) // skip the leading '/'
    {
        size_t end = path.find('/', i);
        if (end == std::string::npos)
            end = path.size();
        if (end > i)
            parts.push_back(path.substr(i, end - i));
        i = end + 1;
    }
    return parts;
}

std::wstring widen(const std::string &s)
{
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

std::string narrow(const std::wstring &w)
{
    if (w.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::string percent_decode(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '%' && i + 2 < s.size() && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2]))
        {
            out += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16);
            i += 2;
        }
        else
        {
            out += s[i];
        }
    }
    return out;
}

// "a=1&b=hello+world&flag" -> {{"a","1"},{"b","hello world"},{"flag",""}}.
// Keys and values are percent-decoded; in a query "+" also means a space.
// A repeated key keeps its last value.
std::map<std::string, std::string> parse_query(const std::string &query)
{
    std::map<std::string, std::string> args;
    for (size_t i = 0; i < query.size();)
    {
        size_t end = query.find('&', i);
        if (end == std::string::npos)
            end = query.size();
        std::string pair = query.substr(i, end - i);
        i = end + 1;
        if (pair.empty())
            continue;
        for (auto &c : pair)
            if (c == '+')
                c = ' ';
        size_t eq = pair.find('=');
        if (eq == std::string::npos)
            args[percent_decode(pair)] = {};
        else
            args[percent_decode(pair.substr(0, eq))] = percent_decode(pair.substr(eq + 1));
    }
    return args;
}

const char *mime_type(const fs::path &p)
{
    std::string ext = p.extension().string();
    for (auto &c : ext)
        c = (char)tolower((unsigned char)c);
    if (ext == ".html" || ext == ".htm")
        return "text/html; charset=utf-8";
    if (ext == ".js" || ext == ".mjs")
        return "text/javascript; charset=utf-8";
    if (ext == ".css")
        return "text/css; charset=utf-8";
    if (ext == ".json")
        return "application/json";
    if (ext == ".svg")
        return "image/svg+xml";
    if (ext == ".png")
        return "image/png";
    if (ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if (ext == ".gif")
        return "image/gif";
    if (ext == ".webp")
        return "image/webp";
    if (ext == ".ico")
        return "image/x-icon";
    if (ext == ".woff")
        return "font/woff";
    if (ext == ".woff2")
        return "font/woff2";
    if (ext == ".wasm")
        return "application/wasm";
    if (ext == ".txt")
        return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

std::optional<response> handle_routes(const std::vector<route> &routes, const std::string &method, const std::string &path,
                                      const std::map<std::string, std::string> &query, const std::string &body)
{
    request req{method, path, query, {}, body};
    auto parts = split_path(path);
    for (const route &r : routes)
        if (r.matches(method, parts, req.params))
            return r.fn(req);
    return std::nullopt;
}

std::optional<response> handle_static(const std::filesystem::path &static_root, const std::string &method, std::string &path,
                                      ComPtr<IStream> &content)
{
    if (method != "GET" && method != "HEAD")
        return std::nullopt;
    if (!static_root.empty())
    {
        if (path == "/")
            path += "index.html";
        std::error_code ec;
        fs::path file = fs::weakly_canonical(static_root / fs::path(path.substr(1)), ec);
        if (ec)
            return std::nullopt;
        auto rel = file.lexically_relative(static_root);
        bool inside = !rel.empty() && *rel.begin() != "..";

        // std::cout << "Serving static file: " << file << " " << rel << " Inside: " << inside << std::endl;

        if (inside && fs::is_regular_file(file, ec) &&
            SUCCEEDED(SHCreateStreamOnFileEx(file.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE,
                                             nullptr, &content)))
        {
            return response{200, mime_type(file), {}};
        }
    }
    return std::nullopt;
}

HRESULT handle_request(ICoreWebView2Environment *env, const std::string &url, const std::vector<route> &routes,
                       const std::filesystem::path &static_root, ICoreWebView2WebResourceRequestedEventArgs *args)
{
    ComPtr<ICoreWebView2WebResourceRequest> request;
    args->get_Request(&request);

    LPWSTR uri_raw = nullptr, method_raw = nullptr;
    request->get_Uri(&uri_raw);
    request->get_Method(&method_raw);
    std::string uri = narrow(uri_raw);
    std::string method = narrow(method_raw);
    CoTaskMemFree(uri_raw);
    CoTaskMemFree(method_raw);

    // "http://app.local/a/b.js?x=1#y" -> "/a/b.js"
    std::string path = uri.substr(url.size());
    size_t cut = path.find_first_of("?#");
    std::string query;
    if (cut != std::string::npos && path[cut] == '?')
        query = path.substr(cut + 1, path.find('#', cut) - (cut + 1));
    path = percent_decode(path.substr(0, cut));
    if (path.empty())
        path = "/";

    // std::cout << "Request URI: " << uri << " Path: " << path << " Method: " << method << " Routes: " << routes.size()
    //   << std::endl;

    ComPtr<IStream> content;
    ComPtr<IStream> body;
    request->get_Content(&body);
    response res = handle_routes(routes, method, path, parse_query(query), read_stream(body.Get()))
                       .or_else([&] { return handle_static(static_root, method, path, content); })
                       .value_or(response{404, "text/plain; charset=utf-8", "Not found: " + path});

    if (res.body.size() > 0)
        content.Attach(SHCreateMemStream(reinterpret_cast<const BYTE *>(res.body.data()), (UINT)res.body.size()));

    std::wstring headers = L"Content-Type: " + widen(res.content_type) + L"\r\nCache-Control: no-store";
    ComPtr<ICoreWebView2WebResourceResponse> response;
    HRESULT hr = env->CreateWebResourceResponse(content.Get(), res.status, res.status == 200 ? L"OK" : L"Not Found",
                                                headers.c_str(), &response);
    if (FAILED(hr))
        return hr;
    return args->put_Response(response.Get());
}

HRESULT on_request(ICoreWebView2Environment *env, const std::string &url, const std::vector<route> &routes,
                   const std::filesystem::path &static_root, ICoreWebView2WebResourceRequestedEventArgs *args)
{
    try
    {
        return handle_request(env, url, routes, static_root, args);
    }
    catch (const std::exception &e)
    {
        std::cerr << "on_request failed: " << e.what() << std::endl;
        return E_FAIL;
    }
    catch (...)
    {
        std::cerr << "on_request failed: unknown exception" << std::endl;
        return E_FAIL;
    }
}

struct webview_impl
{
    ComPtr<ICoreWebView2Controller> webviewController;
    ComPtr<ICoreWebView2> webview;
    HWND hwnd;
    std::wstring url;
    std::vector<route> routes;
    std::filesystem::path static_root;
};

} // namespace

namespace easy_webview
{

webview::webview(const std::string &title)
{
    webview_once::instance();

    auto impl = new webview_impl();
    _impl = impl;

    impl->hwnd = CreateWindowExW(0, L"WebView2Window", std::wstring(title.begin(), title.end()).c_str(), WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800, nullptr, nullptr, GetModuleHandle(nullptr),
                                 std::addressof(impl->webviewController));

    if (!impl->hwnd)
        return;
    ShowWindow(impl->hwnd, SW_SHOW);
    UpdateWindow(impl->hwnd);

    webview_once::register_handle(impl->hwnd);

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [impl](HRESULT result, ICoreWebView2Environment *env) -> HRESULT
            {
                if (FAILED(result))
                {
                    ShowError(impl->hwnd, L"Creating the WebView2 environment failed", result);
                    return result;
                }
                HRESULT hr = env->CreateCoreWebView2Controller(
                    impl->hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [impl](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT
                        {
                            if (FAILED(result) || !controller)
                            {
                                ShowError(impl->hwnd, L"Creating the WebView2 controller failed", result);
                                return result;
                            }
                            impl->webviewController = controller;
                            impl->webviewController->get_CoreWebView2(&impl->webview);

                            RECT bounds;
                            GetClientRect(impl->hwnd, &bounds);
                            impl->webviewController->put_Bounds(bounds);

                            // Hide the status bar that shows link URLs on
                            // hover.
                            ComPtr<ICoreWebView2Settings> settings;
                            if (SUCCEEDED(impl->webview->get_Settings(&settings)))
                                settings->put_IsStatusBarEnabled(FALSE);

                            // Open links that request a new window in this
                            // window instead.
                            EventRegistrationToken token;
                            impl->webview->add_NewWindowRequested(
                                Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                    [](ICoreWebView2 *sender, ICoreWebView2NewWindowRequestedEventArgs *args) -> HRESULT
                                    {
                                        LPWSTR uri = nullptr;
                                        if (SUCCEEDED(args->get_Uri(&uri)))
                                        {
                                            args->put_Handled(TRUE);
                                            sender->Navigate(uri);
                                            CoTaskMemFree(uri);
                                        }
                                        return S_OK;
                                    })
                                    .Get(),
                                &token);

                            const std::string local_url = "http://app.local";

                            ComPtr<ICoreWebView2_2> webview2_2;
                            impl->webview.As(&webview2_2);
                            ComPtr<ICoreWebView2Environment> env;
                            webview2_2->get_Environment(&env);
                            std::wstring filter = widen(local_url + "/*");
                            impl->webview->AddWebResourceRequestedFilter(filter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
                            EventRegistrationToken token_local;
                            impl->webview->add_WebResourceRequested(
                                Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                                    [env, local_url, impl](ICoreWebView2 *,
                                                           ICoreWebView2WebResourceRequestedEventArgs *args) -> HRESULT
                                    { return on_request(env.Get(), local_url, impl->routes, impl->static_root, args); })
                                    .Get(),
                                &token_local);

                            if (!impl->url.empty())
                                impl->webview->Navigate(impl->url.c_str());
                            else
                            {
                                std::wstring wlocal_url = std::wstring(local_url.begin(), local_url.end()) + std::wstring(L"/");
                                impl->webview->Navigate(wlocal_url.c_str());
                            }
                            return S_OK;
                        })
                        .Get());
                return hr;
            })
            .Get());

    if (FAILED(hr))
    {
        MessageBoxW(impl->hwnd,
                    L"Failed to create WebView2 environment.\n"
                    L"Is the WebView2 Runtime installed?",
                    L"Error", MB_ICONERROR);
        return;
    }
}

webview::~webview()
{
    auto impl = reinterpret_cast<webview_impl *>(this->_impl);
    impl->webview.Reset();
    impl->webviewController.Reset();
    delete impl;
}

void webview::set_size(int width, int height)
{
    auto impl = reinterpret_cast<webview_impl *>(this->_impl);
    SetWindowPos(impl->hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);

    UpdateWindow(impl->hwnd);
}

void webview::navigate(const std::string &url)
{

    auto impl = reinterpret_cast<webview_impl *>(this->_impl);
    std::wstring wurl = std::wstring(url.begin(), url.end());
    if (impl->webview)
    {
        impl->webview->Navigate(wurl.c_str());
    }
    else
    {
        impl->url = wurl;
    }
}

void webview::get(const std::string &pattern, handler fn)
{
    reinterpret_cast<webview_impl *>(_impl)->routes.push_back({"GET", split_path(pattern), std::move(fn)});
}

void webview::post(const std::string &pattern, handler fn)
{
    reinterpret_cast<webview_impl *>(_impl)->routes.push_back({"POST", split_path(pattern), std::move(fn)});
}

void webview::put(const std::string &pattern, handler fn)
{
    reinterpret_cast<webview_impl *>(_impl)->routes.push_back({"PUT", split_path(pattern), std::move(fn)});
}

void webview::del(const std::string &pattern, handler fn)
{
    reinterpret_cast<webview_impl *>(_impl)->routes.push_back({"DELETE", split_path(pattern), std::move(fn)});
}

void webview::patch(const std::string &pattern, handler fn)
{
    reinterpret_cast<webview_impl *>(_impl)->routes.push_back({"PATCH", split_path(pattern), std::move(fn)});
}

void webview::head(const std::string &pattern, handler fn)
{
    reinterpret_cast<webview_impl *>(_impl)->routes.push_back({"HEAD", split_path(pattern), std::move(fn)});
}

void webview::serve_static(const std::filesystem::path &root)
{
    reinterpret_cast<webview_impl *>(_impl)->static_root = std::filesystem::weakly_canonical(root);
}

void webview::run() { webview_once::run(); }

} // namespace easy_webview