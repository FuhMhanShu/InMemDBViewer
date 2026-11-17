cmake_minimum_required(VERSION 3.20)
project(InMemDBViewer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

# Qt 6 bevorzugt, sonst Qt 5 (>= 5.15) – NFA-01
find_package(Qt6 COMPONENTS Core Widgets Svg Test QUIET)
if(Qt6_FOUND)
    set(QT_CORE Qt6::Core)
    set(QT_WIDGETS Qt6::Widgets)
    set(QT_TEST Qt6::Test)
    set(QT_SVG Qt6::Svg)
else()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets Svg Test)
    set(QT_CORE Qt5::Core)
    set(QT_WIDGETS Qt5::Widgets)
    set(QT_TEST Qt5::Test)
    set(QT_SVG Qt5::Svg)
endif()

# Optionaler SQLite-Import (FA-21): schaltet INMEMDB_WITH_SQLITE im InMemDB-Unterprojekt an
option(INMEMDBVIEWER_WITH_SQLITE "SQLite-Import aktivieren (INMEMDB_WITH_SQLITE=ON, braucht libsqlite3-dev)" OFF)
if(INMEMDBVIEWER_WITH_SQLITE)
    set(INMEMDB_WITH_SQLITE ON CACHE BOOL "SQLite-Import in InMemDB" FORCE)
endif()

# InMemDB als Unterprojekt (Default: Schwestern-Checkout ../InMemDB; -DINMEMDB_DIR=... zum Uebersteuern)
set(INMEMDB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../InMemDB" CACHE PATH "Pfad zum InMemDB-Repository")
if(NOT EXISTS "${INMEMDB_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "InMemDB nicht gefunden unter '${INMEMDB_DIR}' - bitte -DINMEMDB_DIR=/pfad/zu/InMemDB setzen.")
endif()
add_subdirectory("${INMEMDB_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/InMemDB")

add_library(InMemDBViewer STATIC
    src/viewer_core.cpp
    src/data_input_widget.cpp
    src/data_input_controller.cpp
    src/data_view_widget.cpp
    src/data_view_controller.cpp
    src/document_table_model.cpp
    src/document_tree_model.cpp
    src/graph_scene.cpp
    src/analysis_widget.cpp
    src/analysis_controller.cpp
    src/analysis_model.cpp
)
# AUTOMOC: Header mit Q_OBJECT explizit als Quellen listen, damit moc sie verarbeitet
target_sources(InMemDBViewer PRIVATE
    include/memdbviewer/viewer.hpp
    include/memdbviewer/viewer_core.hpp
    include/memdbviewer/data_input_widget.hpp
    include/memdbviewer/data_view_widget.hpp
    include/memdbviewer/analysis_widget.hpp
    src/import_model.hpp
    src/data_input_controller.hpp
    src/database_model.hpp
    src/document_table_model.hpp
    src/document_tree_model.hpp
    src/graph_scene.hpp
    src/data_view_controller.hpp
    src/discovery_model.hpp
    src/analysis_model.hpp
    src/analysis_controller.hpp
)
target_include_directories(InMemDBViewer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(InMemDBViewer PUBLIC memdb ${QT_WIDGETS} ${QT_SVG})
if(NOT MSVC)
    target_compile_options(InMemDBViewer PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()

    # QtTest-Tests (NFA-05), headless via QT_QPA_PLATFORM=offscreen
    add_executable(viewer_tests tests/main.cpp tests/test_data_input.cpp tests/test_data_view.cpp tests/test_analysis.cpp tests/test_viewer.hpp)
    target_link_libraries(viewer_tests PRIVATE InMemDBViewer ${QT_TEST})
    target_compile_definitions(viewer_tests PRIVATE TESTS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    if(INMEMDBVIEWER_WITH_SQLITE)
        target_compile_definitions(viewer_tests PRIVATE INMEMDB_HAS_SQLITE)
    endif()
    add_test(NAME viewer_tests COMMAND viewer_tests)
    set_tests_properties(viewer_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    option(INMEMDBVIEWER_BUILD_EXAMPLES "App examples/viewer_app bauen" ON)
    if(INMEMDBVIEWER_BUILD_EXAMPLES)
        add_executable(viewer_app examples/viewer_app/main.cpp)
        target_link_libraries(viewer_app PRIVATE InMemDBViewer)
    endif()
endif()
