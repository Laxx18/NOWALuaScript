#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QXmlStreamReader>
#include <QIcon>

#include "luascriptcontroller.h"
#include "luascriptqmladapter.h"
#include "luascriptadapter.h"
#include "appcommunicator.h"
#include "model/luaeditormodel.h"
#include "model/apimodel.h"
#include "qml/luaeditorqml.h"

#include <QQuickWindow>
#include <QDebug>

void activateWindow(const QQmlApplicationEngine& engine)
{
    // Get the root object, which should be the ApplicationWindow or your main window component
    QObject* rootObject = engine.rootObjects().first();

    QQuickWindow* qmlWindow = qobject_cast<QQuickWindow*>(rootObject);
    // Assuming you already have the QQuickWindow pointer (qmlWindow)
    if (qmlWindow)
    {
        // Ensure the window is visible
        qmlWindow->show();

        // Bring the window to the front
        qmlWindow->raise();

        // Set the window to stay on top temporarily
        qmlWindow->setFlag(Qt::WindowStaysOnTopHint, true);

        // Request activation (focus)
        qmlWindow->requestActivate();

        // Temporarily force the window to stay on top
        qmlWindow->setFlag(Qt::WindowStaysOnTopHint, false);

        qmlWindow->showMaximized();
    }
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("NOWA");
    app.setOrganizationDomain("https://lukas-kalinowski.com");
    app.setApplicationName("NOWALuaScript");
#ifdef Q_OS_WIN
    app.setWindowIcon(QIcon(":/icons/NOWALuaScript.ico")); // Windows
#endif

#ifdef Q_OS_LINUX
    app.setWindowIcon(QIcon(":/icons/NOWALuaScript.png")); // Linux
#endif

#ifdef Q_OS_MAC
    app.setWindowIcon(QIcon(":/icons/NOWALuaScript.icns")); // macOS
#endif

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
    }

    QSharedPointer<LuaScriptAdapter> ptrLuaScriptAdapter(new LuaScriptAdapter());
    QSharedPointer<LuaScriptController> ptrLuaScriptController(new LuaScriptController(&engine, ptrLuaScriptAdapter, &app));

    const QUrl url(QStringLiteral("qrc:/qml_files/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
    Qt::QueuedConnection);
    engine.load(url);

#ifdef QT_NO_DEBUG
    if (true == AppCommunicator::isInstanceRunning())
    {
        if (false == initialLuaScriptPath.isEmpty())
        {
            activateWindow(engine);

            AppCommunicator::sendFileToRunningInstance(initialLuaScriptPath);
        }
        return 0;
    }
#endif

    QSharedPointer<AppCommunicator> ptrAppCommunicator(new AppCommunicator(ptrLuaScriptController));

    QObject::connect(ptrAppCommunicator.get(), &AppCommunicator::fileReceived,
                     [=, &engine](const QString& filePath)
                     {
                        ptrLuaScriptController.get()->slot_createLuaScript(filePath);

                        activateWindow(engine);

                     });

    // Delete any existing communication file at start
    ptrAppCommunicator->deleteCommunicationFile();

    // Connect the QCoreApplication's aboutToQuit signal to delete the file when the app closes
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]()
                     {
                        ptrAppCommunicator->deleteRunningFile();
                     });



    // After QML is ready, set a potential first lua script and load it, if it comes from the args
    if (false == initialLuaScriptPath.isEmpty())
    {
        ptrLuaScriptController->slot_createLuaScript(initialLuaScriptPath);
    }

    return app.exec();
}
