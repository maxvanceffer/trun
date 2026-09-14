#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QDebug>

#include <QWKQuick/qwkquickglobal.h>

int main(int argc, char *argv[])
{
    // Needed for window transparency (the blur and rounded corners rely on it).
    QQuickWindow::setDefaultAlphaBuffer(true);

    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    app.setApplicationName(QStringLiteral("FramelessDemo"));
    app.setOrganizationName(QStringLiteral("trun"));

    QQmlApplicationEngine engine;
    QWK::registerTypes(&engine);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("FramelessDemo"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "[FramelessDemo] failed to load QML";
        return -1;
    }

    return app.exec();
}
