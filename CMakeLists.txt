cmake_minimum_required(VERSION 3.28)

project(native_editor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Options — heavy dependencies are opt-in so Stage 0 builds with MSVC only.
# ---------------------------------------------------------------------------
option(ENABLE_QT_UI "Build the Qt 6 Quick/QML shell (requires Qt 6, Stage 1+)" OFF)
option(ENABLE_FFMPEG "Build the real FFmpeg media backend (requires pinned FFmpeg dev libs)" OFF)
option(BUILD_TESTING "Build domain/integration tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(ENABLE_QT_UI)
    # Resolved via CMAKE_PREFIX_PATH (see windows-s1-debug preset -> C:/Qt/...).
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia Widgets)
endif()

if(ENABLE_FFMPEG)
    include(FFmpegPinned)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /EHsc /utf-8)
    # /WX (warnings as errors) is enforced for our own targets below, not
    # globally, so third-party code added later is not broken by our strictness.
endif()

add_subdirectory(src/core)
add_subdirectory(src/persist)
add_subdirectory(src/commands)
add_subdirectory(src/media)
add_subdirectory(src/render)
add_subdirectory(src/export)
add_subdirectory(src/effects)
add_subdirectory(src/ai)
add_subdirectory(src/app)

# Qt shell (Stage 1+). The ONLY module allowed to include Qt headers.
if(ENABLE_QT_UI)
    add_subdirectory(src/shell)
endif()

if(BUILD_TESTING)
    include(CTest)
    add_subdirectory(tests)
endif()

# Enforce warning discipline on first-party targets.
function(editor_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()
