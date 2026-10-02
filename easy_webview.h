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

struct webview
{
    webview(const std::string &title, bool debug = false);
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
