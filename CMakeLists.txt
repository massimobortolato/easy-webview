cmake_minimum_required(VERSION 3.21)
project(easy_webview VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Download the WebView2 SDK (NuGet package is a zip) for the loader library.
set(WEBVIEW2_VERSION "1.0.2903.40")
set(WEBVIEW2_DIR "${CMAKE_BINARY_DIR}/webview2")
if(NOT EXISTS "${WEBVIEW2_DIR}/build/native")
    file(DOWNLOAD
        "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}"
        "${CMAKE_BINARY_DIR}/webview2.zip"
        STATUS dl_status)
    list(GET dl_status 0 dl_code)
    if(NOT dl_code EQUAL 0)
        message(FATAL_ERROR "Failed to download WebView2 SDK: ${dl_status}")
    endif()
    file(MAKE_DIRECTORY "${WEBVIEW2_DIR}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${CMAKE_BINARY_DIR}/webview2.zip"
                    WORKING_DIRECTORY "${WEBVIEW2_DIR}")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WEBVIEW2_ARCH x64)
else()
    set(WEBVIEW2_ARCH x86)
endif()

# --- Library ------------------------------------------------------------------
add_library(easy_webview STATIC easy_webview.cpp easy_webview.h easy_webview_c.cpp easy_webview_c.h)
add_library(easy_webview::easy_webview ALIAS easy_webview)
target_compile_definitions(easy_webview PRIVATE UNICODE _UNICODE)
target_include_directories(easy_webview
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${WEBVIEW2_DIR}/build/native/include")
target_link_libraries(easy_webview PUBLIC
    "${WEBVIEW2_DIR}/build/native/${WEBVIEW2_ARCH}/WebView2LoaderStatic.lib"
    shlwapi
    version)

# --- Example -------------------------------------------------------------------
option(EASY_WEBVIEW_BUILD_EXAMPLE "Build the example application" ${PROJECT_IS_TOP_LEVEL})
if(EASY_WEBVIEW_BUILD_EXAMPLE)
    add_executable(easy_webview_example WIN32 example/main.cpp)
    target_compile_definitions(easy_webview_example PRIVATE UNICODE _UNICODE)
    target_link_libraries(easy_webview_example PRIVATE easy_webview::easy_webview)

    # Embed the static web assets into the executable (see embedded/example_static.h).
    include(cmake/embed_dir.cmake)
    easy_webview_embed_dir(easy_webview_example example_static "${CMAKE_CURRENT_SOURCE_DIR}/example/static")

    # # Copy the static web assets next to the executable on every build.
    # add_custom_target(easy_webview_example_static
    #     COMMAND ${CMAKE_COMMAND} -E copy_directory
    #         "${CMAKE_CURRENT_SOURCE_DIR}/example/static"
    #         "$<TARGET_FILE_DIR:easy_webview_example>/static"
    #     COMMENT "Copying example static files")
    # add_dependencies(easy_webview_example easy_webview_example_static)
endif()
