#include <QGuiApplication>
#include <QCoreApplication>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "app/AppController.hpp"

int main(int argc, char *argv[]) {
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);

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
