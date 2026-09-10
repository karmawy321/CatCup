add_executable(editor
    src/main.cpp
)

target_link_libraries(editor PRIVATE core persist commands media render export effects ai)

if(ENABLE_FFMPEG)
    target_link_libraries(editor PRIVATE media_ffmpeg export_ffmpeg)
    target_compile_definitions(editor PRIVATE WITH_FFMPEG)
    ffmpeg_copy_runtime_dlls(editor)
endif()

if(MSVC)
    target_compile_options(editor PRIVATE /W4 /WX)
endif()

# Stage 1 will add app/qt/ behind ENABLE_QT_UI. This CLI stays as the
# headless acceptance driver (import -> edit -> save -> reopen -> export).
