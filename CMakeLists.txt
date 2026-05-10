cmake_minimum_required(VERSION 3.24)

project(AstraXBrowser
    VERSION 0.1.0
    DESCRIPTION "A portfolio-grade C++ web browser built with Qt WebEngine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Network Widgets WebEngineCore WebEngineWidgets)

qt_standard_project_setup()
include(CTest)

qt_add_executable(astrax-browser
    WIN32
    MACOSX_BUNDLE
    resources.rc
    resources.qrc
    src/main.cpp
    src/core/RequestBlocker.cpp
    src/core/RequestBlocker.h
    src/core/UrlTools.cpp
    src/core/UrlTools.h
    src/engine/CssParser.cpp
    src/engine/CssParser.h
    src/engine/HtmlParser.cpp
    src/engine/HtmlParser.h
    src/engine/LayoutEngine.cpp
    src/engine/LayoutEngine.h
    src/storage/BookmarksStore.cpp
    src/storage/BookmarksStore.h
    src/storage/HistoryStore.cpp
    src/storage/HistoryStore.h
    src/storage/SessionStore.cpp
    src/storage/SessionStore.h
    src/storage/SettingsStore.cpp
    src/storage/SettingsStore.h
    src/ui/BrowserView.cpp
    src/ui/BrowserView.h
    src/ui/BrowserWindow.cpp
    src/ui/BrowserWindow.h
    src/ui/EngineLabWindow.cpp
    src/ui/EngineLabWindow.h
)

target_compile_definitions(astrax-browser
    PRIVATE
        ASTRA_VERSION="${PROJECT_VERSION}"
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
)

target_link_libraries(astrax-browser
    PRIVATE
        Qt6::Network
        Qt6::Widgets
        Qt6::WebEngineCore
        Qt6::WebEngineWidgets
)

if(MSVC)
    target_compile_options(astrax-browser PRIVATE /W4 /permissive-)
else()
    target_compile_options(astrax-browser PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(astra-tests
        tests/CoreStorageTests.cpp
        src/core/UrlTools.cpp
        src/core/UrlTools.h
        src/engine/CssParser.cpp
        src/engine/CssParser.h
        src/engine/HtmlParser.cpp
        src/engine/HtmlParser.h
        src/engine/LayoutEngine.cpp
        src/engine/LayoutEngine.h
        src/storage/BookmarksStore.cpp
        src/storage/BookmarksStore.h
        src/storage/HistoryStore.cpp
        src/storage/HistoryStore.h
        src/storage/SessionStore.cpp
        src/storage/SessionStore.h
        src/storage/SettingsStore.cpp
        src/storage/SettingsStore.h
    )

    target_compile_definitions(astra-tests
        PRIVATE
            QT_NO_CAST_FROM_ASCII
            QT_NO_CAST_TO_ASCII
    )

    target_link_libraries(astra-tests
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(NAME astra-tests COMMAND astra-tests)
endif()
