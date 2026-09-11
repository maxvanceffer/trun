cmake_minimum_required(VERSION 3.16)
project(trun VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 QuickDialogs2 Core Qml Test Svg QuickTest Qml Widgets)
qt_standard_project_setup()

# Frameless window framework (Quick module only; no-op off macOS where unused)
set(QWINDOWKIT_BUILD_WIDGETS OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_QUICK ON CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/qwindowkit)

qt_add_executable(trun-app
    src/main.cpp
    src/projectservice.h
    src/projectservice.cpp
    src/commandexecutor.h
    src/commandexecutor.cpp
    src/logmodel.h
    src/projectlistmodel.h
    src/projectlistmodel.cpp
    src/treemodel.h
    src/treemodel.cpp
    src/settings.h
    src/settings.cpp
    src/mcpserver.h
    src/mcpserver.cpp
    src/mcpagents.h
    src/mcpagents.cpp
)

# Add QML resources via qml module
qt_add_qml_module(trun-app
    URI "trun.app"
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/Sidebar.qml
        qml/Dashboard.qml
        qml/Wizard.qml
        qml/WizardWindow.qml
        qml/CommandCard.qml
        qml/TreeItem.qml
        qml/TreeItemDelegate.qml
        qml/Theme.qml
        qml/IconButton.qml
        qml/StatCard.qml
        qml/DetailPage.qml
        qml/ProgressRing.qml
        qml/RunConfigDialog.qml
        qml/McpSetupDialog.qml
    RESOURCES
        qml.qrc
)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Copy QML files and icons to build directory (for development)
file(COPY ${CMAKE_SOURCE_DIR}/qml DESTINATION ${CMAKE_BINARY_DIR})
add_custom_command(TARGET trun-app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/icons ${CMAKE_BINARY_DIR}/icons
)

target_link_libraries(trun-app PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Core
    Qt6::Svg
    Qt6::Widgets
    QWindowKit::Quick
)

# =====================================================================
# QML Integration Tests (C++ harness loading QML components)
# =====================================================================

qt_add_executable(qmltests
    src/qmltests_main.cpp
    src/qmltests.h
    src/qmltests.cpp
    src/projectservice.h
    src/projectservice.cpp
    src/commandexecutor.h
    src/commandexecutor.cpp
    src/logmodel.h
    src/projectlistmodel.h
    src/projectlistmodel.cpp
    src/treemodel.h
    src/treemodel.cpp
    src/settings.h
    src/settings.cpp
    src/mcpagents.h
    src/mcpagents.cpp
)

# Include QML resources so qrc:/icons/... paths work in tests
# Note: qml.qrc is primarily for the main app (trun-app)
# Tests use file paths directly via build directory copies

# Copy QML files and icons to build directory for QML Test
file(COPY ${CMAKE_SOURCE_DIR}/qml DESTINATION ${CMAKE_BINARY_DIR})
add_custom_command(TARGET qmltests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/icons ${CMAKE_BINARY_DIR}/icons
)

# Copy QML files and icons to build directory for QML Test
file(COPY ${CMAKE_SOURCE_DIR}/qml DESTINATION ${CMAKE_BINARY_DIR})
add_custom_command(TARGET qmltests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/icons ${CMAKE_BINARY_DIR}/icons
)

target_include_directories(qmltests PRIVATE src)

target_link_libraries(qmltests PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Core
    Qt6::Qml
    Qt6::Test
)

install(TARGETS trun-app
    BUNDLE DESTINATION .
)

# Executor tests
add_executable(test_commandexecutor
    src/test_commandexecutor.cpp
    src/commandexecutor.h
    src/commandexecutor.cpp
)
target_include_directories(test_commandexecutor PRIVATE src)
target_link_libraries(test_commandexecutor PRIVATE
    Qt6::Core
    Qt6::Test
)

# MCP agent config tests
add_executable(test_mcpagents
    src/test_mcpagents.cpp
    src/mcpagents.h
    src/mcpagents.cpp
)
target_include_directories(test_mcpagents PRIVATE src)
target_link_libraries(test_mcpagents PRIVATE
    Qt6::Core
    Qt6::Test
)

# MCP server protocol tests
add_executable(test_mcpserver
    src/test_mcpserver.cpp
    src/mcpserver.h
    src/mcpserver.cpp
    src/projectservice.h
    src/projectservice.cpp
    src/commandexecutor.h
    src/commandexecutor.cpp
    src/projectlistmodel.h
    src/projectlistmodel.cpp
    src/treemodel.h
    src/treemodel.cpp
    src/settings.h
    src/settings.cpp
)
target_include_directories(test_mcpserver PRIVATE src)
target_link_libraries(test_mcpserver PRIVATE
    Qt6::Core
    Qt6::Test
)

# Test executable
add_executable(test_projectservice
    src/test_projectservice.cpp
    src/projectservice.h
    src/projectservice.cpp
    src/commandexecutor.h
    src/commandexecutor.cpp
    src/projectlistmodel.h
    src/projectlistmodel.cpp
    src/treemodel.h
    src/treemodel.cpp
    src/settings.h
    src/settings.cpp
)
target_include_directories(test_projectservice PRIVATE src)
target_link_libraries(test_projectservice PRIVATE
    Qt6::Core
    Qt6::Test
)


