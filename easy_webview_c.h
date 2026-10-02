#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ew_webview ew_webview;   // opaque, wraps easy_webview::webview
typedef struct ew_request ew_request;   // opaque, valid only during the handler call
typedef struct ew_response ew_response; // opaque, valid only during the handler call

// An in-memory file for ew_serve_static_files(). The data is not copied and
// must outlive the window.
typedef struct ew_file
{
    const char *path; // URL path, e.g. "/index.html"
    const char *data;
    size_t size;
} ew_file;

// The response starts as 200, "text/plain", empty body.
typedef void (*ew_handler)(const ew_request *req, ew_response *res, void *user_data);

// Values match easy_webview::permission.
typedef enum ew_permission
{
    EW_PERMISSION_UNKNOWN,
    EW_PERMISSION_MICROPHONE,
    EW_PERMISSION_CAMERA,
    EW_PERMISSION_GEOLOCATION,
    EW_PERMISSION_NOTIFICATIONS,
    EW_PERMISSION_OTHER_SENSORS,
    EW_PERMISSION_CLIPBOARD_READ,
    EW_PERMISSION_MULTIPLE_AUTOMATIC_DOWNLOADS,
    EW_PERMISSION_FILE_READ_WRITE,
    EW_PERMISSION_AUTOPLAY,
    EW_PERMISSION_LOCAL_FONTS,
    EW_PERMISSION_MIDI_SYSTEM_EXCLUSIVE_MESSAGES,
    EW_PERMISSION_WINDOW_MANAGEMENT
} ew_permission;

// Values match easy_webview::permission_state.
typedef enum ew_permission_state
{
    EW_PERMISSION_DENY,
    EW_PERMISSION_ALLOW,
    EW_PERMISSION_ASK // let WebView2 show its own prompt
} ew_permission_state;

typedef struct ew_permission_rule
{
    ew_permission kind;
    ew_permission_state state;
} ew_permission_rule;

// --- Window ----------------------------------------------------------------
ew_webview *ew_create(const char *title);
// Like ew_create(); a nonzero debug enables the WebView2 developer tools.
ew_webview *ew_create_debug(const char *title, int debug);
// Like ew_create_debug(), with a permissions table (copied; may be NULL if count
// is 0). It applies only to pages served from https://app.example; every other
// page, and every kind not in the table, is denied.
ew_webview *ew_create_with_permissions(const char *title, int debug, const ew_permission_rule *rules, size_t count);
void ew_destroy(ew_webview *w);

void ew_set_size(ew_webview *w, int width, int height);
void ew_navigate(ew_webview *w, const char *url);

// Shows all windows and blocks until the last one is closed.
void ew_run(void);

// --- Routes ------------------------------------------------------------------
void ew_get(ew_webview *w, const char *pattern, ew_handler fn, void *user_data);
void ew_post(ew_webview *w, const char *pattern, ew_handler fn, void *user_data);
void ew_put(ew_webview *w, const char *pattern, ew_handler fn, void *user_data);
void ew_del(ew_webview *w, const char *pattern, ew_handler fn, void *user_data);
void ew_patch(ew_webview *w, const char *pattern, ew_handler fn, void *user_data);
void ew_head(ew_webview *w, const char *pattern, ew_handler fn, void *user_data);

// --- Static files ------------------------------------------------------------
void ew_serve_static_dir(ew_webview *w, const char *root);
void ew_serve_static_files(ew_webview *w, const ew_file *files, size_t count);

// --- Request accessors ---------------------------------------------------------
// Returned strings are owned by the request and valid only during the handler call.
const char *ew_request_method(const ew_request *req);
const char *ew_request_path(const ew_request *req);
const char *ew_request_body(const ew_request *req, size_t *len); // len may be NULL

// Return NULL if the key is missing.
const char *ew_request_query(const ew_request *req, const char *key);  // "?a=1" -> "a"
const char *ew_request_param(const ew_request *req, const char *key);  // "/:name" -> "name"

// Iterate entries in key order. Return 0 if index is out of range, 1 otherwise.
size_t ew_request_query_count(const ew_request *req);
int ew_request_query_at(const ew_request *req, size_t index, const char **key, const char **value);
size_t ew_request_param_count(const ew_request *req);
int ew_request_param_at(const ew_request *req, size_t index, const char **key, const char **value);

// --- Response setters ----------------------------------------------------------
// All strings are copied.
void ew_response_set_status(ew_response *res, int status);
void ew_response_set_content_type(ew_response *res, const char *content_type);
void ew_response_set_body(ew_response *res, const char *body, size_t len);

// Shorthand for the three setters above, with a NUL-terminated body.
void ew_response_set(ew_response *res, int status, const char *content_type, const char *body);

#ifdef __cplusplus
}
#endif
