#include <QGuiApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "app/AppController.hpp"

int main(int argc, char *argv[]) {
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    app.setApplicationDisplayName(QStringLiteral("AI Authenticity Client"));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/appicon-256.png")));

    QQmlApplicationEngine engine;
    AppController controller;

    engine.rootContext()->setContextProperty("appController", &controller);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    engine.loadFromModule("cliente", "Main");

    return app.exec();
}
