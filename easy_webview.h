#pragma once
#include <filesystem>
#include <functional>
#include <map>
#include <string>

namespace easy_webview
{

struct response
{
    int status;
    std::string content_type;
    std::string body;
};

struct request
{
    std::string method;
    std::string path;
    std::map<std::string, std::string> query;  // "?a=1" -> query["a"]
    std::map<std::string, std::string> params; // "/:name" -> params["name"]
    std::string body;
};

using handler = std::function<response(const request &)>;

// Same order as COREWEBVIEW2_PERMISSION_KIND.
enum class permission
{
    unknown,
    microphone,
    camera,
    geolocation,
    notifications,
    other_sensors,
    clipboard_read,
    multiple_automatic_downloads,
    file_read_write,
    autoplay,
    local_fonts,
    midi_system_exclusive_messages,
    window_management,
};

enum class permission_state
{
    deny,
    allow,
    ask, // let WebView2 show its own prompt
};

struct webview
{
    // `permissions` applies only to pages served from https://app.example; every
    // other page, and every kind not in the table, is denied.
    webview(const std::string &title, bool debug = false, const std::map<permission, permission_state> &permissions = {});
    ~webview();

    void set_size(int width, int height);

    void get(const std::string &pattern, handler fn);
    void post(const std::string &pattern, handler fn);
    void put(const std::string &pattern, handler fn);
    void del(const std::string &pattern, handler fn);
    void patch(const std::string &pattern, handler fn);
    void head(const std::string &pattern, handler fn);
    void serve_static(const std::filesystem::path &root);
    void serve_static(const std::map<std::string, std::string_view> &files);

    void navigate(const std::string &url);
    static void run();
};
} // namespace easy_webview
