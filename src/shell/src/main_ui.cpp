// editor-ui entry point. GUI shell over the Stage 0/1 domain libraries.
//
// Usage:
//   editor-ui [project.json]        open the full editor
//   editor-ui --qml-smoke           load Main.qml headless, exit 0 if clean
//                                   (CI: QT_QPA_PLATFORM=offscreen)

#include "shell/ExportController.hpp"
#include "shell/MediaLibrary.hpp"
#include "shell/Player.hpp"
#include "shell/PreviewItem.hpp"
#include "shell/Selection.hpp"
#include "shell/Session.hpp"
#include "shell/ThumbnailProvider.hpp"
#include "shell/TimelineModel.hpp"

#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickStyle>
#include <QTimer>

#include <iostream>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("native_editor");
    app.setApplicationName("editor-ui");
    QQuickStyle::setStyle("Basic"); // neutral base; themed in QML

    bool smoke = false;
    QString fileToOpen;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--qml-smoke") {
            smoke = true;
        } else if (!arg.startsWith("--")) {
            fileToOpen = arg;
        }
    }

    editor::shell::Session session;
    editor::shell::Selection selection;
    editor::shell::TimelineModel timeline;
    editor::shell::Player player;
    editor::shell::MediaLibrary library;
    editor::shell::ExportController exporter;
    timeline.setSession(&session);
    player.setSession(&session);
    library.setSession(&session);
    exporter.setSession(&session);

    if (!fileToOpen.isEmpty()) {
        session.openFile(QUrl::fromLocalFile(QFileInfo(fileToOpen).absoluteFilePath()));
    }

    // C++-only helper types live in their own URI so QML files that are
    // themselves part of the NativeEditor module never import their own
    // module (circular import -> objectCreationFailed).
    qmlRegisterType<editor::shell::PreviewItem>("EditorCpp", 1, 0, "PreviewItem");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlEngine::warnings, &app,
        [](const QList<QQmlError>& warnings) {
            for (const QQmlError& w : warnings) {
                std::cerr << "QML: " << qPrintable(w.toString()) << "\n";
            }
        },
        Qt::DirectConnection);
    engine.rootContext()->setContextProperty("session", &session);
    engine.rootContext()->setContextProperty("selection", &selection);
    engine.rootContext()->setContextProperty("timeline", &timeline);
    engine.rootContext()->setContextProperty("player", &player);
    engine.rootContext()->setContextProperty("library", &library);
    engine.rootContext()->setContextProperty("exporter", &exporter);
    engine.addImageProvider("thumb", new editor::shell::ThumbnailProvider(&session));

    bool loadFailed = false;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [&] { loadFailed = true; }, Qt::QueuedConnection);
    // QML_FILES live in qml/ so the resource path keeps that segment
    // (qmldir: Main 1.0 qml/Main.qml under :/qt/qml/NativeEditor/).
    engine.load(QUrl("qrc:/qt/qml/NativeEditor/qml/Main.qml"));

    if (smoke) {
        // Let queued creation-failure delivery run, then report.
        QTimer::singleShot(500, &app, &QCoreApplication::quit);
        const int rc = app.exec();
        if (loadFailed) {
            std::cerr << "QML smoke: Main.qml failed to load (see warnings above)\n";
            return 2;
        }
        std::cout << "QML smoke: Main.qml loaded clean\n";
        return rc;
    }
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
