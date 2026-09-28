#pragma once

#include <functional>
#include <map>
#include <string>
#include <filesystem>

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

    class webview
    {
        void *_handle;

    public:
        webview();
        ~webview();

        void set_title(const std::string &title);
        void set_size(int width, int height);

        void get(const std::string &pattern, handler fn);
        void post(const std::string &pattern, handler fn);
        void put(const std::string &pattern, handler fn);
        void del(const std::string &pattern, handler fn);
        void patch(const std::string &pattern, handler fn);
        void head(const std::string &pattern, handler fn);
        void serve_static(const std::filesystem::path &root);
        void run();
    };

} // namespace webview
