#pragma once

#include <QObject>
#include <QQuickWindow>
#include "logmodel.h"
#include "projectlistmodel.h"
#include "projectservice.h"

// Shared context properties (defined in qmltests.cpp)
extern LogModel* g_logModel;
extern ProjectListModel* g_projectListModel;
extern ProjectService* g_projectService;

class QmlTests : public QObject
{
    Q_OBJECT

private slots:
    void test_tree_model_creation();
    void test_add_project_package_json();
    void test_add_project_cargo_toml();
    void test_add_folder_implicit();
    void test_folder_with_child_project();
    void test_qml_sidebar_component_loads();
    void test_qml_tree_item_delegate_loads();
    void test_icons_in_assets();
    void test_qrc_icon_paths_in_qml();
    void test_sidebar_uses_theme();
    void test_console_shows_selected_command();
    void test_console_format_colors();
    void test_tool_plugins();
    void test_dep_offer_opens_dialog();
    void test_sniff_detects_port_from_url();
    void test_detail_page_loads();
    void test_stat_card_loads();
    void test_run_config_dialog_loads();
    void test_mcp_setup_dialog_loads();
    void test_mcp_menu_geometry();
    void test_dashboard_layout_geometry();
    void test_docker_page_cards_span_width();
    void test_busy_label_loads();
    void test_run_all_recent();
};
