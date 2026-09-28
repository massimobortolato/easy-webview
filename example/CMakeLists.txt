cmake_minimum_required(VERSION 3.14.0)
project(easy-webview-example VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(
    easy-webview
    GIT_REPOSITORY https://github.com/massimobortolato/easy-webview.git
    GIT_TAG main
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(easy-webview)

add_executable(easy-webview-example main.cpp)
target_link_libraries(easy-webview-example PRIVATE easy-webview::easy-webview)
set_target_properties(easy-webview-example PROPERTIES
    CXX_EXTENSIONS OFF
    VS_GLOBAL_VcpkgEnabled false)
