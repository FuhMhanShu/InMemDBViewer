cmake_minimum_required(VERSION 3.20)
project(InMemDBViewer VERSION 0.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

# Qt 6 bevorzugt, sonst Qt 5 (>= 5.15) – NFA-01
find_package(Qt6 COMPONENTS Core Widgets Test QUIET)
if(Qt6_FOUND)
    set(QT_CORE Qt6::Core)
    set(QT_WIDGETS Qt6::Widgets)
    set(QT_TEST Qt6::Test)
else()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets Test)
    set(QT_CORE Qt5::Core)
    set(QT_WIDGETS Qt5::Widgets)
    set(QT_TEST Qt5::Test)
endif()

# InMemDB als Unterprojekt (Default: Schwestern-Checkout ../InMemDB; -DINMEMDB_DIR=... zum Uebersteuern)
set(INMEMDB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../InMemDB" CACHE PATH "Pfad zum InMemDB-Repository")
if(NOT EXISTS "${INMEMDB_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "InMemDB nicht gefunden unter '${INMEMDB_DIR}' - bitte -DINMEMDB_DIR=/pfad/zu/InMemDB setzen.")
endif()
add_subdirectory("${INMEMDB_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/InMemDB")

# Optionaler Qt-Modell-Adapter von InMemDB aktivieren (v0.23, nur QtCore):
# INMEMDB_WITH_QT + moc fuer memdb/qt/memdb_qt_model.hpp.
# Wichtig: memdb kompiliert den moc-Output selbst (AUTOMOC), benoetigt daher
# QtCore-Header/Includes am memdb-Target (InMemDBs CMake kennt kein INMEMDB_WITH_QT).
target_link_libraries(memdb PUBLIC ${QT_CORE})
target_compile_definitions(memdb PUBLIC INMEMDB_WITH_QT)
set_target_properties(memdb PROPERTIES AUTOMOC ON)
# PRIVATE: Header nur in libmemdb.a moccen, nicht an verbrauchende Targets propagieren
# (sonst moccen Viewer-Bibliothek/Demo/Tests ihn zusaetzlich -> Mehrfachdefinition beim Link).
target_sources(memdb PRIVATE "${INMEMDB_DIR}/include/memdb/qt/memdb_qt_model.hpp")

add_library(InMemDBViewer STATIC
    src/viewer_controller.cpp
    src/database_model.cpp
    src/collection_item.cpp
    src/schema_scene.cpp
    src/schema_view.cpp
    src/document_view.cpp
    src/document_tree_view.cpp
    src/metrics_panel.cpp
)
# AUTOMOC: Header mit Q_OBJECT explizit als Quellen listen, damit moc sie verarbeitet
target_sources(InMemDBViewer PRIVATE
    include/memdbviewer/collection_item.hpp
    include/memdbviewer/database_model.hpp
    include/memdbviewer/document_tree_view.hpp
    include/memdbviewer/document_view.hpp
    include/memdbviewer/metrics_panel.hpp
    include/memdbviewer/schema_scene.hpp
    include/memdbviewer/schema_view.hpp
    include/memdbviewer/viewer.hpp
    include/memdbviewer/viewer_controller.hpp
)
target_include_directories(InMemDBViewer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(InMemDBViewer PUBLIC memdb ${QT_WIDGETS})
if(NOT MSVC)
    target_compile_options(InMemDBViewer PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()

    # QtTest-Tests (NFA-05), headless via QT_QPA_PLATFORM=offscreen
    add_executable(viewer_tests tests/main.cpp tests/test_viewer.cpp)
    target_link_libraries(viewer_tests PRIVATE InMemDBViewer ${QT_TEST})
    add_test(NAME viewer_tests COMMAND viewer_tests)
    set_tests_properties(viewer_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    option(INMEMDBVIEWER_BUILD_EXAMPLES "Demo-App examples/viewer_demo bauen" ON)
    if(INMEMDBVIEWER_BUILD_EXAMPLES)
        add_executable(viewer_demo examples/viewer_demo/main.cpp)
        target_link_libraries(viewer_demo PRIVATE InMemDBViewer)
    endif()
endif()
