cmake_minimum_required(VERSION 3.14.0)
project(easy-webview VERSION 0.1.0 LANGUAGES C CXX)

add_library(easy-webview STATIC easy_webview.cpp easy_webview.h)
add_library(easy-webview::easy-webview ALIAS easy-webview)

target_include_directories(easy-webview PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>)
target_compile_features(easy-webview PUBLIC cxx_std_23)
set_target_properties(easy-webview PROPERTIES
    CXX_EXTENSIONS OFF
    VS_GLOBAL_VcpkgEnabled false)

if(WIN32)
    target_link_libraries(easy-webview PUBLIC ole32 shell32 shlwapi user32 advapi32 version)
endif()

include(GNUInstallDirs)
install(TARGETS easy-webview EXPORT easy-webviewTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES webview.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT easy-webviewTargets
    NAMESPACE easy-webview::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/easy-webview)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
