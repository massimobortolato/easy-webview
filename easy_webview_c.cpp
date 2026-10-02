#include "easy_webview_c.h"
#include "easy_webview.h"
#include <iterator>

namespace
{
easy_webview::webview *cpp(ew_webview *w) { return reinterpret_cast<easy_webview::webview *>(w); }
const easy_webview::request *cpp(const ew_request *req) { return reinterpret_cast<const easy_webview::request *>(req); }
easy_webview::response *cpp(ew_response *res) { return reinterpret_cast<easy_webview::response *>(res); }

std::string str(const char *s) { return s ? s : ""; }

easy_webview::handler wrap(ew_handler fn, void *user_data)
{
    return [fn, user_data](const easy_webview::request &req)
    {
        easy_webview::response res{200, "text/plain", ""};
        fn(reinterpret_cast<const ew_request *>(&req), reinterpret_cast<ew_response *>(&res), user_data);
        return res;
    };
}

const char *find(const std::map<std::string, std::string> &m, const char *key)
{
    if (!key)
        return nullptr;
    auto it = m.find(key);
    return it != m.end() ? it->second.c_str() : nullptr;
}

int at(const std::map<std::string, std::string> &m, size_t index, const char **key, const char **value)
{
    if (index >= m.size())
        return 0;
    auto it = std::next(m.begin(), index);
    if (key)
        *key = it->first.c_str();
    if (value)
        *value = it->second.c_str();
    return 1;
}
} // namespace

extern "C"
{

ew_webview *ew_create(const char *title)
{
    // Heap-allocated: easy_webview::webview registers its own address and must not move.
    return reinterpret_cast<ew_webview *>(new easy_webview::webview(str(title)));
}

void ew_destroy(ew_webview *w) { delete cpp(w); }

void ew_set_size(ew_webview *w, int width, int height) { cpp(w)->set_size(width, height); }
void ew_navigate(ew_webview *w, const char *url) { cpp(w)->navigate(str(url)); }
void ew_run(void) { easy_webview::webview::run(); }

void ew_get(ew_webview *w, const char *pattern, ew_handler fn, void *user_data)
{
    cpp(w)->get(str(pattern), wrap(fn, user_data));
}

void ew_post(ew_webview *w, const char *pattern, ew_handler fn, void *user_data)
{
    cpp(w)->post(str(pattern), wrap(fn, user_data));
}

void ew_put(ew_webview *w, const char *pattern, ew_handler fn, void *user_data)
{
    cpp(w)->put(str(pattern), wrap(fn, user_data));
}

void ew_del(ew_webview *w, const char *pattern, ew_handler fn, void *user_data)
{
    cpp(w)->del(str(pattern), wrap(fn, user_data));
}

void ew_patch(ew_webview *w, const char *pattern, ew_handler fn, void *user_data)
{
    cpp(w)->patch(str(pattern), wrap(fn, user_data));
}

void ew_head(ew_webview *w, const char *pattern, ew_handler fn, void *user_data)
{
    cpp(w)->head(str(pattern), wrap(fn, user_data));
}

void ew_serve_static_dir(ew_webview *w, const char *root) { cpp(w)->serve_static(std::filesystem::path(str(root))); }

void ew_serve_static_files(ew_webview *w, const ew_file *files, size_t count)
{
    std::map<std::string, std::string_view> m;
    for (size_t i = 0; i < count; ++i)
        if (files[i].path)
            m[files[i].path] = std::string_view(files[i].data ? files[i].data : "", files[i].data ? files[i].size : 0);
    cpp(w)->serve_static(m);
}

const char *ew_request_method(const ew_request *req) { return cpp(req)->method.c_str(); }
const char *ew_request_path(const ew_request *req) { return cpp(req)->path.c_str(); }

const char *ew_request_body(const ew_request *req, size_t *len)
{
    if (len)
        *len = cpp(req)->body.size();
    return cpp(req)->body.c_str();
}

const char *ew_request_query(const ew_request *req, const char *key) { return find(cpp(req)->query, key); }
const char *ew_request_param(const ew_request *req, const char *key) { return find(cpp(req)->params, key); }

size_t ew_request_query_count(const ew_request *req) { return cpp(req)->query.size(); }
size_t ew_request_param_count(const ew_request *req) { return cpp(req)->params.size(); }

int ew_request_query_at(const ew_request *req, size_t index, const char **key, const char **value)
{
    return at(cpp(req)->query, index, key, value);
}

int ew_request_param_at(const ew_request *req, size_t index, const char **key, const char **value)
{
    return at(cpp(req)->params, index, key, value);
}

void ew_response_set_status(ew_response *res, int status) { cpp(res)->status = status; }
void ew_response_set_content_type(ew_response *res, const char *content_type) { cpp(res)->content_type = str(content_type); }

void ew_response_set_body(ew_response *res, const char *body, size_t len)
{
    cpp(res)->body.assign(body ? body : "", body ? len : 0);
}

void ew_response_set(ew_response *res, int status, const char *content_type, const char *body)
{
    cpp(res)->status = status;
    cpp(res)->content_type = str(content_type);
    cpp(res)->body = str(body);
}

} // extern "C"
