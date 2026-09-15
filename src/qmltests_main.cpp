#include "qmltests.h"
#include <QGuiApplication>
#include <QQuickStyle>
#include <QTest>
#include <QFileInfo>

static void ensure_globals()
{
    if (!g_logModel) {
        g_logModel = new LogModel();
        g_projectListModel = new ProjectListModel();
        g_projectService = new ProjectService(g_projectListModel);
    }
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Same style as the app: control metrics must match production
    QQuickStyle::setStyle("Basic");

    ensure_globals();

    return QTest::qExec(new QmlTests, argc, argv);
}
