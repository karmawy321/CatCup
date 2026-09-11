# Qt Quick shell (Stage 1). The ONLY module allowed to include Qt headers.
# Headless `editor` stays Qt-free; `editor-ui` is the interactive editor.

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC OFF)

qt_policy(SET QTP0001 NEW) # default :/qt/qml resource prefix (matches main_ui load path)
qt_policy(SET QTP0004 NEW)

qt_add_executable(editor-ui
    src/main_ui.cpp
    src/Session.cpp
    src/TimelineModel.cpp
    src/Selection.cpp
    src/FrameCompositor.cpp
    src/PreviewItem.cpp
    src/Player.cpp
    src/MediaLibrary.cpp
    src/ThumbnailProvider.cpp
    src/ExportController.cpp
    # Headers listed explicitly: AUTOMOC only scans target sources, and the
    # Q_OBJECT classes live in include/shell/.
    include/shell/Session.hpp
    include/shell/TimelineModel.hpp
    include/shell/Selection.hpp
    include/shell/FrameCompositor.hpp
    include/shell/PreviewItem.hpp
    include/shell/Player.hpp
    include/shell/MediaLibrary.hpp
    include/shell/ThumbnailProvider.hpp
    include/shell/ExportController.hpp
)

qt_add_qml_module(editor-ui
    URI NativeEditor
    VERSION 1.0
    QML_FILES
        qml/Theme.qml
        qml/StudioButton.qml
        qml/StudioSlider.qml
        qml/StudioCard.qml
        qml/TimecodeDisplay.qml
        qml/Main.qml
        qml/MediaBrowser.qml
        qml/PreviewView.qml
        qml/Inspector.qml
        qml/TimelineView.qml
        qml/ExportDialog.qml
)

target_include_directories(editor-ui PRIVATE include)

target_link_libraries(editor-ui
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Multimedia
        core
        persist
        commands
        media
        media_ffmpeg
        render
        export
        export_ffmpeg
        effects
        ai
)

if(MSVC)
    target_compile_options(editor-ui PRIVATE /W4 /WX /wd4702)
    target_compile_options(editor-ui PRIVATE /experimental:external /external:W0)
endif()


ffmpeg_copy_runtime_dlls(editor-ui)

# Dev-run convenience: so ./editor-ui finds Qt plugins/DLLs without install.
set_target_properties(editor-ui PROPERTIES
    VS_DEBUGGER_ENVIRONMENT "PATH=C:/Qt/6.8.3/msvc2022_64/bin;$ENV{PATH}\nQT_QPA_PLATFORM=windows")
