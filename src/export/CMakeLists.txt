add_library(export STATIC
    src/ExportJob.cpp
)

target_include_directories(export PUBLIC include)
target_link_libraries(export PUBLIC core render)

if(MSVC)
    target_compile_options(export PRIVATE /W4 /WX)
endif()

# Real MP4 encoder (libx264 + aac) behind the same job state machine.
if(ENABLE_FFMPEG)
    add_subdirectory(ffmpeg)
endif()
