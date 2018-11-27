#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QMetaType>
#include <QDebug>
#include "client.h"
#include "mid.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);


    qmlRegisterType<mid>("he.qt.client", 1, 0, "Net");
    typedef QMap<QString, QString> peer_info_map;
    qRegisterMetaType<peer_info_map>("peer_info_map");

    QQmlApplicationEngine engine;
    //client c_net;
    //engine.rootContext()->setContextProperty("net", &c_net);

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
