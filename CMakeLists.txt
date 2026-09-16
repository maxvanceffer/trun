cmake_minimum_required(VERSION 3.16)

# Release version: override from CI with -DTRUN_VERSION=<tag without v>
if(NOT DEFINED TRUN_VERSION)
    set(TRUN_VERSION 0.1.0)
endif()
project(trun VERSION ${TRUN_VERSION} LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 QuickDialogs2 Core Qml Test Svg QuickTest Qml Widgets Network)
qt_standard_project_setup()

# Frameless window framework (Quick module only; no-op off macOS where unused)
set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_WIDGETS OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_QUICK ON CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/qwindowkit)

qt_add_executable(trun-app MACOSX_BUNDLE
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
    src/systemstats.h
    src/systemstats.cpp
    src/databaseservice.h
    src/databaseservice.cpp
    src/dockerservice.h
    src/dockerservice.cpp
    src/updater.h
    src/updater.cpp
    src/mactitlebar.h
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
        qml/UpdateDialog.qml
        qml/UpdateAlert.qml
        qml/TrafficLights.qml
        qml/DatabasesPage.qml
        qml/DockerPage.qml
    RESOURCES
        qml.qrc
)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Wizard artwork (dark plate + Retina). Loaded via qrc:/assets/wizard/...
# (qml.qrc itself is only embedded as a blob by the QML module above,
# so files needed at runtime must be compiled here explicitly.)
qt_add_resources(trun-app "wizard_art"
    PREFIX "/"
    FILES
        assets/wizard/wizard-bg.png
        assets/wizard/wizard-bg@2x.png
)

# Copy QML files and icons next to the binary (for development).
# With MACOSX_BUNDLE that is Contents/MacOS, mirroring the release layout.
file(COPY ${CMAKE_SOURCE_DIR}/qml DESTINATION ${CMAKE_BINARY_DIR})
if(APPLE)
    add_custom_command(TARGET trun-app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/qml $<TARGET_FILE_DIR:trun-app>/qml
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/icons $<TARGET_FILE_DIR:trun-app>/icons
    )
else()
    add_custom_command(TARGET trun-app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/icons ${CMAKE_BINARY_DIR}/icons
    )
endif()

target_link_libraries(trun-app PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Core
    Qt6::Svg
    Qt6::Widgets
    Qt6::Network
    QWindowKit::Quick
)

# Version visible in code (QApplication::applicationVersion), overridable via -DTRUN_VERSION=
target_compile_definitions(trun-app PRIVATE TRUN_VERSION="${TRUN_VERSION}")

if(APPLE)
    set_target_properties(trun-app PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.maxvanceffer.trun"
        MACOSX_BUNDLE_BUNDLE_NAME "trun"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE "trun"
        # Output file name: trun.app (target stays trun-app).
        OUTPUT_NAME "trun"
    )
    # Dock/Finder icon: assets/icons/trun.icns -> Contents/Resources.
    set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/assets/icons/trun.icns")
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(trun-app PRIVATE ${APP_ICON_MACOS})
    # main.cpp loads QML/icons from files next to the binary
    # (exePath + "/qml/main.qml", exePath + "/icons"), so ship them in the bundle.
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/qml"
        DESTINATION "trun.app/Contents/MacOS")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/icons"
        DESTINATION "trun.app/Contents/MacOS")
endif()

if(APPLE)
    target_sources(trun-app PRIVATE src/mactitlebar.mm src/databaseservice_macos.cpp src/macappmenu.h src/macappmenu.mm)
    target_link_libraries(trun-app PRIVATE "-framework AppKit")
elseif(UNIX)
    target_sources(trun-app PRIVATE src/databaseservice_linux.cpp)
endif()

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

# Updater logic tests (pure functions, no network)
add_executable(test_updater
    src/test_updater.cpp
    src/updater.h
    src/updater.cpp
    src/settings.h
    src/settings.cpp
    src/commandexecutor.h
    src/commandexecutor.cpp
)
target_include_directories(test_updater PRIVATE src)
target_link_libraries(test_updater PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
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
    src/dockerservice.h
    src/dockerservice.cpp
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

# Multi-root workspace tests (append on scan, per-root stale drop, migration)
add_executable(test_roots
    src/test_roots.cpp
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
target_include_directories(test_roots PRIVATE src)
target_link_libraries(test_roots PRIVATE
    Qt6::Core
    Qt6::Test
)


