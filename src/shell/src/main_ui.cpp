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
#include <QQuickWindow>
#include <QTimer>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif

#include <QIcon>
#include <iostream>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("CatCup");
    app.setApplicationName("CatCup");
    QQuickStyle::setStyle("Basic"); // neutral base; themed in QML

    app.setWindowIcon(QIcon(":/qt/qml/NativeEditor/assets/images/catcup_icon.png"));

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

    bool hadCriticalWarnings = false;
    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlEngine::warnings, &app,
        [&hadCriticalWarnings](const QList<QQmlError>& warnings) {
            for (const QQmlError& w : warnings) {
                std::cerr << "QML: " << qPrintable(w.toString()) << "\n";
                const QString msg = w.description();
                if (msg.contains("ReferenceError") || msg.contains("TypeError") ||
                    msg.contains("Unable to assign [undefined]") ||
                    msg.contains("is not defined") || msg.contains("Cannot assign to non-existent property")) {
                    hadCriticalWarnings = true;
                }
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

#if defined(Q_OS_WIN)
    for (auto* obj : engine.rootObjects()) {
        if (auto* window = qobject_cast<QQuickWindow*>(obj)) {
            HWND hwnd = reinterpret_cast<HWND>(window->winId());
            if (hwnd) {
                BOOL darkMode = TRUE;
                // DWMWA_USE_IMMERSIVE_DARK_MODE (20 on Win10 20H1+ / Win11; 19 on older Win10)
                DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
                DwmSetWindowAttribute(hwnd, 19, &darkMode, sizeof(darkMode));
                COLORREF captionColor = RGB(0x1E, 0x1E, 0x22); // Theme.bgSidebar
                DwmSetWindowAttribute(hwnd, 35, &captionColor, sizeof(captionColor));
                COLORREF textColor = RGB(0xFF, 0xFF, 0xFF);
                DwmSetWindowAttribute(hwnd, 36, &textColor, sizeof(textColor));
            }
        }
    }
#endif

    if (smoke) {
        // Let queued creation-failure delivery run, then report.
        QTimer::singleShot(500, &app, &QCoreApplication::quit);
        const int rc = app.exec();
        if (loadFailed || hadCriticalWarnings) {
            std::cerr << "QML smoke: Main.qml failed to load clean (see warnings above)\n";
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
