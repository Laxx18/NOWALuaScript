#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QXmlStreamReader>

#include "luascriptcontroller.h"
#include "luascriptqmladapter.h"
#include "luascriptadapter.h"
#include "appcommunicator.h"
#include "model/luaeditormodel.h"
#include "model/apimodel.h"
#include "qml/luaeditorqml.h"

#include <QDebug>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("NOWA");
    app.setOrganizationDomain("https://lukas-kalinowski.com");
    app.setApplicationName("NOWALuaScript");

    QQmlApplicationEngine engine;
    // For custom modules
    engine.addImportPath(QStringLiteral("qrc:/qml_files"));

    qmlRegisterSingletonType<LuaScriptQmlAdapter>("NOWALuaScript", 1, 0, "LuaScriptQmlAdapter", LuaScriptQmlAdapter::getSingletonTypeProvider);
    qmlRegisterSingletonType<LuaEditorModel>("NOWALuaScript", 1, 0, "NOWALuaEditorModel", LuaEditorModel::getSingletonTypeProvider);
    qmlRegisterUncreatableType<LuaEditorModelItem>("NOWALuaScript", 1, 0, "LuaScriptModelItem", "Not ment to be created in qml.");
    qmlRegisterSingletonType<ApiModel>("NOWALuaScript", 1, 0, "NOWAApiModel", ApiModel::getSingletonTypeProvider);
    // Register LuaEditorQml as a QML type
    qmlRegisterType<LuaEditorQml>("NOWALuaScript", 1, 0, "LuaEditorQml");

    QString initialLuaScriptPath;
    // Handle initial Lua script path from command-line arguments
    if (argc > 1)
    {
        initialLuaScriptPath = argv[1]; // First argument is the Lua script file path
        qDebug() << "Opening Lua script at:" << initialLuaScriptPath;
        // Handle the initial Lua script (open in editor)

        // showWindowsMessageBox("Received via args Lua Script", "Received Lua script at: " + initialLuaScriptPath);
    }

    QSharedPointer<LuaScriptAdapter> ptrLuaScriptAdapter(new LuaScriptAdapter());
    QSharedPointer<LuaScriptController> ptrLuaScriptController(new LuaScriptController(&engine, ptrLuaScriptAdapter, &app));

    const QUrl url(QStringLiteral("qrc:/qml_files/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
    Qt::QueuedConnection);
    engine.load(url);

    // Install the native event filter
    QSharedPointer<AppCommunicator> ptrAppCommunicator(new AppCommunicator(ptrLuaScriptController));
    // app.installNativeEventFilter(ptrAppCommunicator.get());

    // Connect the QCoreApplication's aboutToQuit signal to delete the file when the app closes
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]()
                     {
                         if (QFile::remove(ptrAppCommunicator->getRunningFilePath()))
                         {
                             qDebug() << "Running file deleted successfully";
                         }
                         else
                         {
                             qWarning() << "Failed to delete running file";
                         }
                     });

#ifdef QT_NO_DEBUG
    if (true == ptrAppCommunicator->isInstanceRunning())
    {
        qDebug() << "Instance is already running.";
        return 0;
    }
#endif

    // After QML is ready, set a potential first lua script and load it, if it comes from the args
    if (false == initialLuaScriptPath.isEmpty())
    {
        ptrLuaScriptController->slot_createLuaScript(initialLuaScriptPath);
    }

    return app.exec();
}
