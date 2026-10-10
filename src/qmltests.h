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
    void test_hybrid_folder_gets_manifests_entry();
    void test_top_level_root_gets_manifests_entry();
    void test_manifests_entry_stack_on_second_manifest();
    void test_manifests_entry_when_subfolder_comes_first();
    void test_qml_sidebar_component_loads();
    void test_qml_tree_item_delegate_loads();
    void test_icons_in_assets();
    void test_qrc_icon_paths_in_qml();
    void test_sidebar_uses_theme();
    void test_console_shows_selected_command();
    void test_console_run_segments();
    void test_console_parameters_option_kinds();
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
    void test_api_import_curl();
    void test_api_history_clear();
    void test_api_draft_save();
    void test_api_save_request_update();
    void test_api_update_request_status();
    void test_api_append_history();
    void test_api_suggest_name();
    void test_api_page_list_groups();
    void test_api_page_open_request();
    void test_api_page_keys();
    void test_api_request_urlbar();
    void test_api_request_tabs();
    void test_header_save_tail();
    void test_app_dialog_hosting();
    void test_ui_list_element_click_through();
};
