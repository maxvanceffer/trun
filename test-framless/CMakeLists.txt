cmake_minimum_required(VERSION 3.19)
project(frameless-demo VERSION 0.1.0 LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Gui Qml Svg)
qt_standard_project_setup()

# Vendored QWindowKit (Quick module only)
set(QWINDOWKIT_BUILD_WIDGETS OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_QUICK ON CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/qwindowkit
                 ${CMAKE_CURRENT_BINARY_DIR}/qwindowkit)

qt_add_executable(frameless-demo
    src/main.cpp
)

qt_add_qml_module(frameless-demo
    URI FramelessDemo
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Sidebar.qml
        qml/TrafficLights.qml
)

# Keep the module root flat so loadFromModule("FramelessDemo", "Main") resolves.
foreach(_f Main Sidebar TrafficLights)
    set_source_files_properties(qml/${_f}.qml PROPERTIES QT_RESOURCE_ALIAS ${_f}.qml)
endforeach()

target_link_libraries(frameless-demo PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Gui
    Qt6::Svg
    QWindowKit::Quick
)
