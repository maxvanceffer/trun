#include "treemodel.h"
#include <QtQuickTest>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include "projectservice.h"
#include "logmodel.h"
#include "projectlistmodel.h"

QML_DECLARE_TYPE(QmlTreeModel)
QML_DECLARE_TYPE(ProjectListModel)

static LogModel* g_logModel = nullptr;
static ProjectListModel* g_projectListModel = nullptr;
static ProjectService* g_projectService = nullptr;

extern "C" Q_QMLTEST_EXPORT QObject* testSetupFunction()
{
    if (!g_logModel) {
        g_logModel = new LogModel();
        g_projectListModel = new ProjectListModel();
        g_projectService = new ProjectService(g_projectListModel);
    }
    return nullptr;
}

extern "C" Q_QMLTEST_EXPORT void testCleanupFunction()
{
    g_logModel = nullptr;
    g_projectListModel = nullptr;
    g_projectService = nullptr;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<QmlTreeItem>("Trun.Models", 1, 0, "QmlTreeItem");
    qmlRegisterType<QmlTreeModel>("Trun.Models", 1, 0, "QmlTreeModel");
    qmlRegisterType<ProjectListModel>("Trun.Models", 1, 0, "ProjectListModel");

    return quick_test_main(argc, argv, "tst_treeview", nullptr);
}
